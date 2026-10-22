"""编译型客户端的原生事务接入。导入和包校验均不访问 USB。"""
import hashlib
import base64
import os
from pathlib import Path, PurePosixPath
import re
import secrets
import stat
import time

from native_adapter import Journal, NativeAdapter, check, strict_json
from native_bootstrap import NativeBootstrap
from native_operations import NativeOperations, Payload
from native_client_resources import RESOURCE_HASHES, REQUIRED_MODULES, PLATFORM


def journal_root():
    home = Path(os.environ.get('LOCALAPPDATA', str(Path.home()/'AppData/Local'))) if PLATFORM == 'windows' else Path.home()/'Library/Application Support'
    return home/'X2DInstaller/native-transactions'


def begin_action(installer, action):
    check(action in ('Install','Uninstall','Diagnose'), 'client-action')
    if action != 'Diagnose':
        import native_receipt_cleanup
        native_receipt_cleanup.assert_no_pending(journal_root())
    # 只有用户新操作才分配新作用域；同一次调用中的所有阶段复用它。
    # 未确认操作内没有后台重连、重新分配 attempt 或自动重发。
    installer._native_action_id = secrets.token_hex(8)
    installer._native_action = action


def digest(data):
    return hashlib.sha256(data).hexdigest()


def ordinary(root, name):
    check(isinstance(name, str) and '\\' not in name and ':' not in name and '\0' not in name, 'package-path')
    parts = name.split('/')
    check(parts and all(p not in ('', '.', '..') for p in parts) and not PurePosixPath(name).is_absolute(), 'package-path')
    path = root
    for index, part in enumerate(parts):
        path = path/part
        s = path.lstat()
        check(not stat.S_ISLNK(s.st_mode) and not (getattr(s, 'st_file_attributes', 0) & 0x400), 'package-link')
        check(stat.S_ISDIR(s.st_mode) if index < len(parts)-1 else stat.S_ISREG(s.st_mode), 'package-file-type')
    return path


def select_profile(metadata, chosen, action):
    profiles=metadata.get('profiles')
    if profiles is not None:
        check(isinstance(profiles,dict) and set(profiles)=={'248','255'}, 'package-profile-set')
        if action=='Uninstall':chosen=248
        check(type(chosen) is int and chosen in (248,255), 'package-profile-selection')
        metadata=profiles[str(chosen)]
        check(metadata['schema']=='X2D_NATIVE_CLIENT_V1' and metadata['firmware']=='4.2.0', 'profile-scope')
    check(type(chosen) is int and chosen in (248,255) and chosen == metadata['stock']['feature_mask'], 'native-feature-selection-not-supported')
    return metadata


def verify_package(installer):
    root = installer.root
    if PLATFORM == 'windows':
        # 电脑端只检查安装必需文件；不遍历额外文件，不打开说明/许可证 TXT。
        # 真正写入相机的 Payload/Bootstrap 摘要及机内回读仍由对应事务验证。
        required = sorted(set(RESOURCE_HASHES) | set(REQUIRED_MODULES) | {
            'HasselbladTuningPatchInstaller.exe', 'X2dEngine.dll',
            'X2dTransport.dll', 'python311.dll', 'runtime/python311.zip',
            'universal-package.txt', 'package-release.json',
        })
        for index, name in enumerate(required):
            ordinary(root, name)
            installer.progress('PATCH_PROGRESS=preflight|'+str(index+1)+'|'+str(len(required)))
        metadata = strict_json(ordinary(root, 'native/client-metadata.json').read_text(encoding='utf8'))
        check(metadata['schema'] == 'X2D_NATIVE_CLIENT_V1' and metadata['firmware'] == '4.2.0', 'package-scope')
        chosen = installer.pending_mask if installer.pending else installer.feature_mask
        metadata = select_profile(metadata, chosen, getattr(installer, '_native_action', None))
        installer.stock = metadata['stock']
        installer.full_stock = installer.stock
        installer.removal = metadata['removal']
        installer.legacy = {'targets': {}}
        installer.previous = []
        installer.native_metadata = metadata
        return
    raw = ordinary(root, 'package-integrity.json').read_bytes()
    check(len(raw) <= 16*1024*1024, 'package-manifest-size')
    integrity = strict_json(raw.decode('utf-8-sig'))
    check(isinstance(integrity, dict) and 0 < len(integrity) <= 20000, 'package-integrity')
    # 全部模块在原生启动器验清单后才导入；资源清单另有编译内置锚。
    check(set(RESOURCE_HASHES) <= set(integrity) and set(REQUIRED_MODULES) <= set(integrity), 'package-required-files')
    check(all(integrity[n] == h for n, h in RESOURCE_HASHES.items()), 'package-resource-anchor')
    actual = set()
    for path in root.rglob('*'):
        s = path.lstat()
        check(not stat.S_ISLNK(s.st_mode) and not (getattr(s, 'st_file_attributes', 0) & 0x400), 'package-extra-link')
        if stat.S_ISREG(s.st_mode):
            actual.add(path.relative_to(root).as_posix())
        else:
            check(stat.S_ISDIR(s.st_mode), 'package-extra-type')
    # Windows 启动器是自身清单的外部信任根，不能形成自摘要循环。
    # Mac 启动器位于 Resources 外，由 Contents 级原生清单约束。
    outer = {'X2dStart.exe'} if PLATFORM == 'windows' else set()
    check(actual == set(integrity) | {'package-integrity.json'} | outer, 'package-extra-or-missing')
    for index, (name, expected) in enumerate(integrity.items()):
        check(isinstance(expected, str) and re.fullmatch('[0-9a-f]{64}', expected), 'package-file-digest')
        check(digest(ordinary(root, name).read_bytes()) == expected, 'package-file-changed:'+name)
        installer.progress('PATCH_PROGRESS=preflight|'+str(index+1)+'|'+str(len(integrity)))
    metadata = strict_json(ordinary(root, 'native/client-metadata.json').read_text(encoding='utf8'))
    check(metadata['schema'] == 'X2D_NATIVE_CLIENT_V1' and metadata['firmware'] == '4.2.0', 'package-scope')
    chosen = installer.pending_mask if installer.pending else installer.feature_mask
    metadata=select_profile(metadata,chosen,getattr(installer,'_native_action',None))
    installer.stock = metadata['stock']
    installer.full_stock = installer.stock
    installer.removal = metadata['removal']
    installer.legacy = {'targets': {}}
    installer.previous = []
    installer.native_metadata = metadata


def operations(installer):
    check(hasattr(installer, 'native_metadata'), 'package-not-checked')
    # 日志是本机状态，不写包目录；不使用相机序列号作为文件名。
    scope = getattr(installer, '_native_action_id', None)
    check(isinstance(scope, str) and re.fullmatch('[0-9a-f]{16}', scope), 'native-action-not-started')
    journal = journal_root()/scope
    setup_key=installer.native_metadata.get('setup_resource','native/x2d-native-setup')
    check(setup_key in RESOURCE_HASHES, 'setup-resource-anchor')
    setup = ordinary(installer.root, setup_key).read_bytes()
    boot = NativeBootstrap(installer, journal/'bootstrap', setup,
                           RESOURCE_HASHES[setup_key],
                           installer.native_metadata.get('bootstrap_previous_sha256', ()))
    adapter = NativeAdapter(installer, Journal(journal/'operations'), boot.verify, boot.verify_reader)
    return NativeOperations(installer, adapter, boot)


def select_uninstall_helper(installer):
    """同包卸载复用当前 helper；跨版本仍走基础表和精确历史摘要检查。"""
    check(getattr(installer, '_native_action', None) in ('Uninstall', 'Install'), 'uninstall-helper-action')
    key = 'native/client-metadata.json'
    raw = ordinary(installer.root, key).read_bytes()
    check(digest(raw) == RESOURCE_HASHES[key], 'uninstall-metadata-anchor')
    metadata = strict_json(raw.decode('utf8'))
    profiles = metadata.get('profiles')
    if profiles is None:
        return
    check(set(profiles) == {'248', '255'}, 'uninstall-profile-set')
    def removal(profile):
        rows = profile['removal']['targets']
        result = {r['path']: (r['original'], r['mode'], frozenset(r['allowed'])) for r in rows}
        check(len(result) == len(rows), 'uninstall-removal-duplicates')
        return result
    check(removal(profiles['248']) == removal(profiles['255']), 'uninstall-removal-scope-mismatch')
    check(profiles['248']['stock']['required_guards'] == profiles['255']['stock']['required_guards'],
          'uninstall-guard-scope-mismatch')
    path = '/system/bin/x2d-native-setup'
    reply = installer.one('p='+path+';if test -L "$p";then echo LINK;elif test -f "$p";then sha256sum "$p";elif test -e "$p";then echo OTHER;else echo ABSENT;fi')
    check(reply == 'ABSENT' or re.fullmatch('[0-9a-f]{64}  '+re.escape(path), reply),
          'uninstall-helper-file-state')
    selected = profiles['248']
    if reply != 'ABSENT':
        current = reply.split(' ')[0]
        matches = [p for p in profiles.values()
                   if RESOURCE_HASHES[p.get('setup_resource', 'native/x2d-native-setup')] == current]
        check(len(matches) <= 1, 'uninstall-helper-ambiguous')
        if matches:
            selected = matches[0]
    installer.native_metadata = selected
    installer.stock = selected['stock']
    installer.full_stock = installer.stock
    installer.removal = selected['removal']
    installer.log.append('卸载辅助程序档位='+str(installer.stock['feature_mask']))


def payload(installer, kind):
    check(kind in ('install-stock', 'uninstall'), 'client-payload-kind')
    name='native-install.gz' if kind=='install-stock' else 'native-uninstall.gz'
    key=installer.native_metadata.get('payload_prefix','native/')+name
    check(key in RESOURCE_HASHES, 'payload-resource-anchor')
    return Payload(kind, ordinary(installer.root, key).read_bytes(), RESOURCE_HASHES[key],
                   installer.native_metadata['manifest_sha256'], installer.native_metadata['stock']['feature_mask'])


def cleanup(installer):
    directories = installer.one('ls -d /system/x2d-un-retained-???????? 2>/dev/null; true').splitlines()
    check(len(directories) <= 8 and len(set(directories)) == len(directories), 'retained-directory-count')
    for directory in directories:
        match = re.fullmatch('/system/x2d-un-retained-([0-9a-f]{8})', directory)
        check(match is not None, 'retained-directory-name')
        operations(installer).retained_cleanup('/data/local/tmp/x2d-un-'+match[1])


def cleanup_data(installer, mode):
    result = operations(installer).data_cleanup(mode)
    installer.log.append('原生清理：'+('延期，现场保留' if result.deferred else '本次回执完成'))
    if result.deferred:
        raise RuntimeError('清理尚未完成：'+result.fields.get('outcome', 'unknown')+
                           '；保留现场，不继续安装或删除辅助程序。')
    return result


def cleanup_tool(installer):
    # 只能在重启后保留文件和data清理均已结束时调用；之后不再执行setup。
    return operations(installer).bootstrap.cleanup()


def archive_uninstall_receipts(installer, *, remove=False):
    import native_receipt_cleanup
    journal = operations(installer).bootstrap.directory.parent
    if remove:
        verify_targets(installer, installer.removal['targets'], original=True)
    location = native_receipt_cleanup.run(installer, journal, remove=remove)
    installer.log.append('卸载回执已保存到电脑'+('，机内回执和空事务锁已清除。' if remove else '。'))
    return location


def verify_targets(installer, targets, original=False):
    # 类型快照仅用于选路；这里独立全 SHA 回读，不将 FILE 视为正确内容。
    for item in targets:
        path = item['path']
        check(re.fullmatch('/system/[A-Za-z0-9_./-]+', path) and '..' not in path.split('/'), 'verify-target-path')
        expected = item['original'] if original else item['candidate_sha256']
        command = 'f='+path+'; if test -L "$f"; then echo LINK; elif test -f "$f"; then sha256sum "$f"; elif test -e "$f"; then echo OTHER; else echo ABSENT; fi'
        check(len(command.encode()) <= 231, 'verify-command-limit')
        reply = installer.one(command)
        check(reply == 'ABSENT' if expected == 'ABSENT' else reply == expected+'  '+path, 'verify-target-mismatch:'+path)


def prepare_uninstall(installer):
    """在更新事务辅助程序前退出现有 worker 的提前准备；不删除任务锁。"""
    installer.status('退出扩展拍摄准备，等待后台归还缓存')
    directory='/tmp/x2d-pixel-overclock'
    status=directory+'/status.json'
    lock='/tmp/x2d-multishot-active'
    def lock_state():
        return installer.one('p='+lock+';if test -L "$p";then echo LINK;elif test -d "$p";then echo HELD;elif test -e "$p";then echo OTHER;else echo FREE;fi')
    def read_worker():
        value=installer.one('p='+status+';if test -L "$p";then echo LINK;elif test -f "$p";then /system/xbin/busybox base64 "$p";else echo ABSENT;fi')
        if value=='ABSENT':return None,None
        check(len(value)<131072 and value!='LINK','uninstall-worker-status-file')
        raw=base64.b64decode(''.join(value.split()),validate=True)
        state=strict_json(raw.decode('utf8'))
        check(state.get('protocol')==3 and all(type(state.get(k)) is bool for k in
              ('busy','prepared','preparing')),'uninstall-worker-status-shape')
        check(not state['busy'],'相机正在拍摄或处理，未退出准备，也未开始卸载')
        return state,raw
    held=lock_state()
    check(held in ('HELD','FREE'),'uninstall-task-lock-type')
    state,raw=read_worker()
    if held=='FREE' and (state is None or not (state['prepared'] or state['preparing'])):return
    check(state is not None and (state['prepared'] or state['preparing']),
          '任务锁仍占用且未确认提前准备；保留现场，未开始卸载')
    worker='/system/bin/x2d-pixel-overclock/worker'
    known={h for item in installer.removal['targets'] if item['path']==worker
           for h in item.get('allowed',[]) if re.fullmatch('[0-9a-f]{64}',h)}
    actual=installer.one('test ! -L '+worker+' && sha256sum '+worker)
    check(any(actual==h+'  '+worker for h in known),'准备中的后台版本未登记，请先从相机界面退出扩展拍摄')
    check(installer.property('camera','exposure_status')=='0' and
          installer.property('error','active_error')=='false' and
          installer.property('system','system_state')=='1','uninstall-prepare-camera-not-idle')
    def identity():
        pid=installer.one('pidof worker')
        check(re.fullmatch('[1-9][0-9]{0,8}',pid) is not None,'uninstall-worker-pid')
        check(installer.one('readlink /proc/'+pid+'/exe')==worker,'uninstall-worker-executable')
        inodes=installer.one("stat -Lc '%d:%i' /proc/"+pid+'/exe '+worker).splitlines()
        check(len(inodes)==2 and inodes[0]==inodes[1] and
              re.fullmatch('[0-9]+:[0-9]+',inodes[0]) is not None,'uninstall-worker-inode')
        proc=installer.one('/system/xbin/busybox base64 /proc/'+pid+'/stat')
        proc=base64.b64decode(''.join(proc.split()),validate=True).decode('ascii')
        check(proc.startswith(pid+' (') and ')' in proc,'uninstall-worker-stat')
        fields=proc.rsplit(')',1)[1].split()
        check(len(fields)>19 and fields[0]!='Z' and fields[19].isdigit(),'uninstall-worker-start')
        return pid,fields[19],inodes[0]
    incarnation=identity()
    state,_=read_worker()
    check(state is not None,'uninstall-worker-disappeared')
    session=state.get('session')
    ack=state.get('ack')
    check(isinstance(session,str) and re.fullmatch('[0-9a-f]{8}',session) is not None and
          type(ack) is int and 0<=ack<=4294967295 and state.get('shutter_pending') is False,
          'uninstall-worker-session-or-pending')
    check(identity()==incarnation,'uninstall-worker-replaced')
    # 仅操作已有后台协议文件；不启停服务，不触发拍摄，不强删任务锁。
    # 写前绑定稳定的会话/请求编号和非拍摄状态；心跳增长不导致误拒绝。
    command='d='+directory+';test ! -L "$d" && test -d "$d" && test -f "$d/arm.txt" && test ! -L "$d/arm.txt" && echo READY'
    check(installer.one(command)=='READY','uninstall-arm-path')
    pattern='"session":"'+session+'","ack":'+str(ack)+',"busy":false,.*"shutter_pending":false'
    command="cd "+directory+" && grep -q '"+pattern+"' status.json && printf 'DISARMED\\n' >arm.txt && echo DISARMED"
    check(len(command.encode('ascii'))<=231,'uninstall-disarm-command-limit')
    check(installer.one(command)=='DISARMED','uninstall-disarm-unconfirmed')
    installer.log.append('已发送一次退出准备请求；卸载事务尚未开始。')
    check(identity()==incarnation,'uninstall-worker-replaced-after-disarm')
    for _ in range(100):
        held=lock_state()
        check(held in ('HELD','FREE'),'uninstall-task-lock-type')
        state,_raw=read_worker()
        check(state is not None,'uninstall-worker-disappeared')
        check(state.get('session')==session and state.get('ack')==ack and
              state.get('shutter_pending') is False,'uninstall-worker-session-changed')
        if held=='FREE' and not state['prepared'] and not state['preparing']:
            check(installer.property('storage','storage_status')=='0','uninstall-storage-not-ready')
            check(identity()==incarnation,'uninstall-worker-replaced-after-release')
            installer.log.append('扩展准备已正常退出，后台已释放任务锁；未强制删除锁。')
            return
        time.sleep(.1)
    raise ValueError('退出扩展拍摄准备超时；未开始卸载，保留现场')


def assert_fresh_install_stage(installer):
    # 不以旧 result=1 推断可重试；失败/不完整事务需要保留诊断。
    command = 'cd /data/local/tmp/x2d-rs1 && { for f in payload started done result backup; do test ! -e "$f" && test ! -L "$f" || exit 3; done; echo NATIVE_STAGE_UNUSED; }'
    check(installer.one(command) == 'NATIVE_STAGE_UNUSED', 'previous-install-unconfirmed-preserved')


def verify_ui(installer):
    contract=installer.native_metadata.get('native_ui')
    check(isinstance(contract,dict) and contract.get('schema')=='X2D_NATIVE_UI_CONTRACT_V1','native-ui-contract-missing')
    check(type(contract.get('required_capabilities')) is int and 0<contract['required_capabilities']<2**64,'native-ui-capabilities')
    # Fixed native command validates its compiled module/GUI hashes, current
    # PID incarnation and a new nonce reply. No UI setter, install or retry.
    reply=installer.one('/system/bin/x2d-native-flow ui-ready')
    check(reply=='NATIVE_UI_READY_CURRENT_REQUEST','native-ui-current-readiness-unconfirmed')
    installer.log.append('原生界面：本次请求、当前进程和载荷版本核对通过。')
