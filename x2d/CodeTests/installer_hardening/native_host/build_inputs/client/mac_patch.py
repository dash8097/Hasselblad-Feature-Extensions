"""第一代 X2D 4.2.0 的 macOS 纯 USB 安装编排。

复用 R5.8.20 相机事务，不增加摄影或网络控制。导入和构造不连接硬件。
只由用户按钮触发 run；每一步失败即停止，不自动重发或重启。
"""
import base64
import hashlib
import json
import re
import secrets
import subprocess
import time
from datetime import datetime
from pathlib import Path
import mac_usb
import mac_diagnostics

class PatchError(RuntimeError):
    pass

def require(condition, message):
    if not condition:
        raise PatchError(message)

def sha(data):
    return hashlib.sha256(data).hexdigest()

DATA_SPACE_COMMAND = '''v=$(df -k /data 2>/dev/null); r=$?; printf '%s\\n' "$v" | awk -v r="$r" '$NF=="/data"{n++;v=$(NF-2)} END{if(r==0&&n==1&&v~/^[0-9]+$/)print "DATA_FREE_KIB="v;else print "DATA_FREE_UNREADABLE"}' '''.rstrip()

def parse_data_space(reply):
    match = re.fullmatch(r'DATA_FREE_KIB=([0-9]+)', reply.strip())
    require(match is not None, 'BINARY_SPACE_UNREADABLE：未读到 /data 的有效剩余空间，不代表空间不足；上传尚未开始。')
    free = int(match[1])
    require(free <= 9223372036854775807, 'BINARY_SPACE_UNREADABLE：空间回复数值超出范围。')
    require(free > 32768, 'BINARY_SPACE_INSUFFICIENT：/data 剩余 '+str(free)+' KiB，需要大于32768 KiB（32 MiB）；上传尚未开始。')
    return free

def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))

def protected(text):
    text = re.sub(r'(?i)(serial(?:number)?|serial_number|device_serial|password|secret|token|key)\s*[:=]\s*\S+', r'\1=[已隐藏]', str(text))
    text = re.sub(r'(?i)(?:/mnt/[^\s\"\']+|[A-Z]:[\\/][^\r\n]+)', '[本地或照片路径已隐藏]', text)
    text = re.sub(r'\b[A-Z]{2}\d{8}\b', '[设备标识已隐藏]', text)
    text = re.sub(r'(?i)\bB\d{4,}\.(?:jpg|jpeg|heif|heic|3fr|raw)\b', '[照片名已隐藏]', text)
    text = re.sub(r"printf '%s' '[A-Za-z0-9+/=]+'", '传输载荷分块（数据省略）', text)
    return text[:8192] + ('\n[超过诊断上限，已截断]' if len(text) > 8192 else '')

def snapshot_command(path):
    require(bool(re.fullmatch(r'/system/[A-Za-z0-9_./-]+', path)) and not re.search(r'/\.\.?(/|$)', path), '组件路径无效')
    command = 'f='+path+'; if test -L "$f"; then echo UNSUPPORTED_LINK; elif test -f "$f"; then echo FILE; elif test -e "$f"; then echo UNSUPPORTED_TYPE; else echo ABSENT; fi'
    mac_usb.shell_request(command, 1)
    return command

def view(manifest, original=False):
    return {path: ('ABSENT' if original and not manifest['original'].get(key) else 'FILE') for key,path in manifest['targets'].items()}
def matches(snapshot, expected):
    return all(snapshot.get(path)==('ABSENT' if value=='ABSENT' else 'FILE') for path,value in expected.items())
def removal_route(snapshot, removal):
    for item in removal['targets']:
        require(snapshot.get(item['path']) in ('FILE','ABSENT'), '组件状态不完整或不是普通文件：'+item['path'])
    return 'KnownProject' if any(item['original']=='ABSENT' and snapshot[item['path']]=='FILE' for item in removal['targets']) else 'Stock'
def route(snapshot, stock, legacy, previous, removal):
    require(all(snapshot.get(p)=='FILE' for p in stock['required_guards']), '原厂必需文件缺失')
    return removal_route(snapshot,removal)

from package_feature_selection import select_features,installed_mask,INSTALLED_MASK_COMMAND

class Installer:
    def __init__(self, root, library=None, run_batch=None, progress=None):
        self.root = Path(root)
        self.library = library
        self.transport = run_batch
        self.progress = progress or (lambda text: None)
        self.pending = ''
        self.feature_mask = 255
        self.pending_mask = None
        self.stage = '等待操作'
        self.log = []
        self.first_failure = ''
        self.lease = None
        self.context = {}

    def status(self, text):
        self.stage = text
        self.context['stage'] = text
        self.progress(text)

    def batch(self, commands):
        for command in commands:
            mac_usb.shell_request(command, 1)
        self.context.update(command='\n'.join(protected(c) for c in commands), actual='本次请求尚未回复', transport='请求中')
        try:
            replies = (self.transport or (lambda c: mac_usb.batch(self.library, c)))(commands)
        except Exception as exc:
            m = re.search(r'BATCH_STEP_(\d+)_',str(exc))
            if m and int(m[1]) < len(commands):
                self.context['command'] = protected(commands[int(m[1])])
            if 'SHELL_REMOTE_FAILURE' in str(exc):
                self.context.update(transport='相机已回复，命令返回失败；不是 USB 无回复', camera_exit='非零，协议未提供具体退出码',actual='本条远端命令失败；本批后续命令未发送')
            else:
                self.context['transport'] = '未取得完整回复；没有重发请求'
            raise
        require(len(replies) == len(commands), 'USB 回复数量不完整')
        self.context['transport'] = '收到完整回复；不等于事务完成'
        self.context['actual'] = ('原厂错误服务回复（私有字段省略）' if any('Properties.GetAll' in c for c in commands) else
                                  '后台状态回复（私有字段省略）' if any('status.json' in c for c in commands) else
                                  '开机状态已读取（续接摘要不进入日志）' if any('random/boot_id' in c for c in commands) else
                                  protected('\n'.join(replies)))
        return replies

    def one(self, command):
        return self.batch([command])[0].strip()

    def check_package(self):
        integrity = read(self.root/'package-integrity.json')
        require(isinstance(integrity, dict) and integrity, '安装包完整性清单无效')
        for name, digest in integrity.items():
            path = self.root/name
            require(not Path(name).is_absolute() and '..' not in Path(name).parts and '\\' not in name, '安装包路径无效')
            require(path.is_file() and not path.is_symlink() and re.fullmatch('[0-9a-f]{64}', digest), '安装文件缺失：'+name)
            require(sha(path.read_bytes()) == digest, '安装文件损坏或版本混用：'+name)
            self.progress('PATCH_PROGRESS=preflight|'+str(list(integrity).index(name)+1)+'|'+str(len(integrity)))
        for name in ('feature-catalog.json','package_feature_selection.py','cleanup_patch_data_r33.sh','mac_usb.py','mac_patch.py','mac_gui.py','mac_diagnostics.py','diagnostic-commands.json','payload/install-payload.tar.gz',
                     'payload/install-manifest.json','payload/install-validation.json','uninstall/payload.tar.gz',
                     'uninstall/manifest.json','uninstall/validation.json','uninstall/cleanup-validation.json',
                     'known-installations/legacy-components.json','usb/arm64/libusb-1.0.dylib','usb/x86_64/libusb-1.0.dylib',
                     'runtime/arm64/bin/python3.12','runtime/x86_64/bin/python3.12'):
            require(name in integrity, '完整性清单缺项：'+name)
        self.stock = read(self.root/'payload/install-manifest.json')
        self.legacy = read(self.root/'known-installations/legacy-components.json')
        self.previous = [read(p) for p in sorted((self.root/'known-installations').glob('released-*.json'))]
        self.removal = read(self.root/'uninstall/manifest.json')
        proof = read(self.root/'payload/install-validation.json')
        require(proof['script_sha256'] == self.stock['install_sha256'] and proof['payload_files'] == self.stock['files'], '安装事务回归证据与载荷不符')
        for file, key in [('validation.json','script_sha256'),('cleanup-validation.json','cleanup_sha256')]:
            proof = read(self.root/'uninstall'/file)
            require(proof.get('offline') and proof.get('device_requests') == 0 and proof['script_sha256'] == self.removal[key], '卸载事务证据不符')
        require(sha((self.root/'payload/install-payload.tar.gz').read_bytes()) == self.stock['archive_sha256'], '安装归档校验失败')
        require(sha((self.root/'uninstall/payload.tar.gz').read_bytes()) == self.removal['archive_sha256'], '卸载归档校验失败')
        self.full_stock=self.stock
        self.stock=select_features(self.stock,self.pending_mask if self.pending else self.feature_mask)

    def collect_snapshot(self, phase='snapshot'):
        paths = sorted(set(self.stock['required_guards']) | set(self.stock['targets'].values()) |
                       set(self.legacy['targets'].values()) | {x['path'] for x in self.removal['targets']})
        snapshot = {}
        for offset in range(0, len(paths), 8):
            selection = paths[offset:offset+8]
            replies = self.batch([snapshot_command(p) for p in selection])
            for path, reply in zip(selection, replies):
                reply = reply.strip()
                if reply == 'ABSENT':
                    snapshot[path] = 'ABSENT'
                else:
                    require(reply == 'FILE', '组件回复不完整或路径为链接：'+path)
                    snapshot[path] = 'FILE'
            self.status('正在检测组件 '+str(min(offset+8,len(paths)))+'/'+str(len(paths)))
            self.progress('PATCH_PROGRESS='+phase+'|'+str(min(offset+8,len(paths)))+'|'+str(len(paths)))
        return snapshot

    def check_usb_prompt(self):
        # Diagnose 在 run 中提前返回；仅安装/卸载检查自动确认已知 USB 提示。
        self.status('USB 连接提醒检查')
        command = '/bin/dbus-send --system --print-reply --dest=com.hasselblad.error /error org.freedesktop.DBus.Properties.GetAll string:com.hasselblad.error | /system/xbin/busybox base64'
        summary = mac_diagnostics.native_error(self.one(command))
        self.context['actual'] = summary
        if summary.startswith('原厂错误标志=true；CODE=40；CAT=5；ACTION=1；'):
            require(not self.context.get('usb_prompt_ack_attempted'), '本次操作已发送过 USB 提示确认，不自动重发。')
            self.context['usb_prompt_ack_attempted'] = True
            self.status('自动跳过大容量存储提示')
            reply = self.one('/bin/dbus-send --system --print-reply --reply-timeout=3000 --dest=com.hasselblad.error /error com.hasselblad.error.ack_error int32:40')
            require(bool(re.fullmatch(r'method return[^\r\n]*', reply)), 'USB 提示确认回执未取得；未重发。')
            self.status('回读 USB 提示状态')
            after = mac_diagnostics.native_error(self.one(command))
            self.context['actual'] = after
            require(after.startswith('原厂错误标志=false；'), 'USB 提示确认后仍未就绪；未重发。'+after)
            self.context['actual'] = '已自动跳过大容量存储提示；不代表安装或拍摄成功'

    def screen_get(self):
        command = '/bin/dbus-send --system --print-reply --dest=com.hasselblad.system /system org.freedesktop.DBus.Properties.Get string:com.hasselblad.system string:screen_idle_timeout'
        reply = self.one(command)
        m = re.search(r'variant\s+int32\s+(-?[0-9]+)\s*$', reply)
        require(m is not None and int(m[1]) >= -1, '未取得完整息屏设置')
        return int(m[1])

    def screen_set(self, value):
        require(isinstance(value, int) and value >= -1, '息屏参数无效')
        reply = self.one('/bin/dbus-send --system --print-reply --dest=com.hasselblad.system /system org.freedesktop.DBus.Properties.Set string:com.hasselblad.system string:screen_idle_timeout variant:int32:'+str(value))
        require(bool(re.fullmatch(r'method return[^\r\n]*', reply)), '息屏设置回复未确认')

    def lease_start(self):
        original = self.screen_get()
        self.lease = (original, original not in (0, -1))
        if self.lease[1]:
            self.screen_set(0)
            require(self.screen_get() == 0, '安装期间关闭自动息屏未确认，未开始事务')

    def lease_stop(self):
        lease, self.lease = self.lease, None
        if lease and lease[1]:
            current = self.screen_get()
            if current == 0:
                self.screen_set(lease[0])
                require(self.screen_get() == lease[0], '息屏恢复未确认，请在相机电源设置中核对')
            elif current != lease[0]:
                self.log.append('息屏值已由外部改变，保留当前值='+str(current))

    def boot(self):
        reply = self.one('sha256sum /proc/sys/kernel/random/boot_id')
        m = re.fullmatch(r'([0-9a-f]{64})\s+/proc/sys/kernel/random/boot_id', reply)
        require(m is not None, '开机状态未确认，更新未继续')
        return m[1]

    def ensure_stage(self, stage):
        require(stage == '/data/local/tmp/x2d-rs1' or bool(re.fullmatch(r'/data/local/tmp/x2d-un-[0-9a-f]{8}', stage)), '暂存路径无效')
        for directory in ['/data','/data/local','/data/local/tmp',stage]:
            reply = self.one('d='+directory+'; if test -L "$d"; then echo LINK; elif test -d "$d"; then echo DIRECTORY; elif test -e "$d"; then echo OTHER; else echo ABSENT; fi')
            if reply == 'DIRECTORY':
                continue
            require(reply == 'ABSENT' and directory != '/data', '暂存目录不可用：'+directory+'；已有文件保留')
            require(self.one('d='+directory+'; test ! -e "$d" && test ! -L "$d" && mkdir -m700 "$d" && echo STACK_DIRECTORY_CREATED') == 'STACK_DIRECTORY_CREATED', '暂存目录创建未确认')

    def file_state(self, stage, name):
        require(bool(re.fullmatch(r'/data/local/tmp/x2d-(?:rs1|un-[0-9a-f]{8})', stage)) and bool(re.fullmatch('[A-Za-z0-9_.-]+', name)), '暂存文件路径无效')
        reply = self.one('cd '+stage+' && (f='+name+'; test ! -L "$f" && if test -f "$f"; then sha256sum "$f"; elif test -e "$f"; then echo UNSUPPORTED_TYPE; else echo ABSENT; fi)')
        if reply == 'ABSENT':
            return reply
        m = re.fullmatch(r'([0-9a-f]{64})\s+'+re.escape(name), reply)
        require(m is not None, '暂存文件状态不可用：'+name)
        return m[1]

    def upload(self, data, stage, token, expected):
        """只发送 ASCII base64；全程同一控制通道。断开后不重发。

        续传必须以远端长度及本地相同前缀 SHA-256 双重确认；不可盲目追加。
        """
        require(sha(data) == expected and bool(re.fullmatch('[a-z0-9-]{1,30}', token)), '上传载荷校验失败')
        if self.file_state(stage, token+'.gz') == expected:
            return
        require(self.file_state(stage, token+'.gz') == 'ABSENT', '同名上传文件内容不同，原文件保留')
        encoded = base64.b64encode(data).decode('ascii')
        reply = self.one('cd '+stage+' && (f='+token+'.b64; if test -L "$f"; then echo LINK; elif test -f "$f"; then wc -c < "$f"; sha256sum "$f"; elif test -e "$f"; then echo OTHER; else echo ABSENT; fi)')
        offset = 0
        if reply == 'ABSENT':
            self.one('cd '+stage+' && (set -C; : > '+token+'.b64)')
        else:
            m = re.fullmatch(r'([0-9]+)\s*\n([0-9a-f]{64})\s+'+re.escape(token+'.b64'), reply)
            require(m is not None, '续传前缀回复无效，未追加')
            offset = int(m[1])
            require(offset <= len(encoded) and sha(encoded[:offset].encode('ascii')) == m[2], '续传前缀不同，已有暂存保留')
        chunk = min(170, 231-len("printf '%s' '' >> "+stage+'/'+token+'.b64'))
        require(chunk > 0, '上传命令过长')
        start = time.monotonic()
        for pos in range(offset, len(encoded), chunk*24):
            commands = ["printf '%s' '"+encoded[i:i+chunk]+"' >> "+stage+'/'+token+'.b64'
                        for i in range(pos, min(pos+chunk*24,len(encoded)), chunk)]
            self.batch(commands)
            done = min(pos+chunk*24, len(encoded))
            self.status('USB 传输 '+str(done*100//len(encoded))+'% · '+str(done*3//4//1024)+' / '+str(len(data)//1024)+' KB；请保持连接')
            self.progress('PATCH_PROGRESS=transfer|'+str(min(len(data),done*3//4))+'|'+str(len(data)))
        require(self.file_state(stage, token+'.b64') == sha(encoded.encode('ascii')), '收到的编码文件校验不符，未解包')
        self.one('cd '+stage+' && (set -C; /system/xbin/busybox base64 -d '+token+'.b64 > '+token+'.gz)')
        require(self.file_state(stage, token+'.gz') == expected, '收到的归档校验不符，未解包')
        self.log.append('USB 载荷完整校验通过；本次传输 '+str(round(time.monotonic()-start))+' 秒（非速度保证）')

    def stage_idle(self):
        return self.one('cd /data/local/tmp/x2d-rs1 && (for f in started done result backup; do if test -e "$f" || test -L "$f"; then echo TRANSACTION; exit; fi; done; echo STACK_STAGE_IDLE)')

    def preserve_stage(self):
        stage = '/data/local/tmp/x2d-rs1'
        done, result = self.batch(['cd '+stage+' && (test ! -L done && if test -f done; then cat done; else test ! -e done && echo ABSENT; fi)', 'cd '+stage+' && test ! -L result && test -f result && cat result'])
        complete = done.strip() in ('RAW_STACK_BACKEND_INSTALLED_NO_CAPTURE','RAW_STACK_STOCK_FILES_INSTALLED_MANUAL_REBOOT_REQUIRED') and result.strip() == '0'
        failed = done.strip() == 'ABSENT' and result.strip() == '1'
        require(complete or failed, '之前的安装尚未确认结束；暂存与备份保留，请读取诊断')
        snapshot = self.collect_snapshot()
        require(matches(snapshot, view(self.stock, True)), '之前的事务未恢复原厂文件，保留现场')
        if failed:
            for path in self.stock['targets'].values():
                require(self.one('f='+path+'; test ! -e "$f.rs-next" && test ! -L "$f.rs-next" && test ! -e "$f.rs-rollback" && test ! -L "$f.rs-rollback" && echo CLEAN') == 'CLEAN', '之前失败事务仍有未确认临时文件')
            require(self.one('pidof camera-service >/dev/null && pidof camera-gui >/dev/null && echo SERVICES_RUNNING') == 'SERVICES_RUNNING', '恢复后的服务未确认')
            require(re.search(r'(^|,)ro(,|$)', self.one('awk \'$2=="/system"{print $4}\' /proc/mounts')), '系统只读状态未确认')
            for service, prop, value in [('camera','exposure_status','0'),('storage','storage_status','0'),('error','active_error','false'),('system','system_state','1')]:
                require(self.property(service, prop) == value, '失败事务恢复状态未确认：'+prop)
        require(self.one('test ! -e /tmp/x2d-multishot-active && echo STACK_LOCK_RELEASED') == 'STACK_LOCK_RELEASED', '相机任务活动，之前的暂存保留')
        suffix = secrets.token_hex(4)
        command = 'd='+stage+'; q=$d.saved-'+suffix+'; test ! -L $d && test ! -e $q && test ! -L $q && test ! -e /tmp/x2d-multishot-active && test "$(cat $d/result)" = '+('1' if failed else '0')+' && mv $d $q && echo STAGE_SAVED'
        require(self.one(command) == 'STAGE_SAVED', '之前的暂存归档未确认')
        require(self.one('mkdir -m700 '+stage+' && echo STAGE_CREATED') == 'STAGE_CREATED', '新暂存目录创建未确认')

    def data_cleanup(self, mode='before'):
        require(mode in ('before','installed','uninstalled'), '清理阶段无效')
        self.status('清理旧暂存；保留当前失败诊断')
        stage = '/data/local/tmp/x2d-tidy-'+secrets.token_hex(4)
        for directory in ('/data','/data/local','/data/local/tmp'):
            self.status('初始化清理暂存父目录'); self.context['target'] = directory
            state = self.one('d='+directory+'; if test -L "$d"; then echo LINK; elif test -d "$d"; then echo DIRECTORY; elif test -e "$d"; then echo OTHER; else echo ABSENT; fi')
            if state == 'DIRECTORY': continue
            require(state == 'ABSENT' and directory != '/data', '清理父目录不可用：'+directory+'；状态='+state)
            reply = self.one('d='+directory+'; if mkdir -m700 "$d"; then echo DIRECTORY_CREATED; else echo DIRECTORY_CREATE_FAILED; fi')
            require(reply == 'DIRECTORY_CREATED', '创建清理父目录失败：'+directory+'；回复='+reply)
        self.status('清理旧暂存；保留当前失败诊断'); self.context['target'] = stage
        reply = self.one('d='+stage+'; if test -e "$d" || test -L "$d"; then echo TIDY_STAGE_EXISTS; elif mkdir -m700 "$d"; then echo TIDY_STAGE_READY; else echo TIDY_STAGE_CREATE_FAILED; fi')
        require(reply == 'TIDY_STAGE_READY', '清理暂存创建未确认：'+reply)
        data = (self.root/'cleanup_patch_data_r33.sh').read_bytes()
        encoded = base64.b64encode(data).decode('ascii')
        commands = ["printf '%s' '"+encoded[i:i+128]+"' >> "+stage+'/s.b64' for i in range(0,len(encoded),128)]
        for i in range(0,len(commands),24): self.batch(commands[i:i+24])
        require(self.one('cd '+stage+' && /system/xbin/busybox base64 -d s.b64 > s.sh && sh -n s.sh && sha256sum s.sh') == sha(data)+'  s.sh', '清理脚本传输不完整，未执行')
        command = 'cd '+stage+' && test ! -e started && (set -C; : >started) && (nohup sh -c \'sh ./s.sh '+mode+' "$PWD" >log 2>&1; echo $? >exit\' </dev/null >/dev/null 2>&1 & echo TIDY_DISPATCHED)'
        require(self.one(command) == 'TIDY_DISPATCHED', '后台清理派发未确认，没有重发')
        start = time.monotonic(); previous = ''; summary = None
        while time.monotonic()-start < 300:
            reply = self.one('cd '+stage+' && if test -f exit; then cat exit; tail -n 3 log; else cat progress 2>/dev/null || echo TIDY_RUNNING; fi')
            terminal = re.match(r'^([0-9]+)\n', reply)
            if terminal:
                self.context.update(camera_exit=terminal[1],camera_log=reply)
                require(terminal[1] == '0', '后台清理失败，现场已保留：'+reply)
                summaries = [s for s in reply.splitlines() if re.fullmatch(r'DATA_CLEANUP_(COMPLETE removed=\d+ retained=\d+ free_before_kib=\d+ free_after_kib=\d+|DEFERRED_CAPTURE|DEFERRED_OTHER_CLEANUP)',s)]
                require(len(summaries) == 1, '清理退出但完成回执缺失')
                summary = summaries[0]
                require(mode != 'before' or 'DEFERRED' not in summary, '相机任务或其他清理正在进行，本次安装未开始；清理未重发')
                break
            if reply != previous:
                previous = reply; self.status('清理旧暂存：'+reply)
                progress = re.search(r'scanned=(\d+) total=(\d+)',reply)
                if progress: self.progress('PATCH_PROGRESS='+('cleanup-pre' if mode=='before' else 'cleanup-post')+'|'+progress[1]+'|'+progress[2])
            time.sleep(.5)
        require(summary is not None, '清理结束回执未取得，进度和日志保留，没有重发')
        self.log.append(summary)
        self.one('d='+stage+'; test ! -L "$d" && test -d "$d" && rm -rf "$d" && echo TIDY_TOOL_REMOVED')

    def cleanup(self):
        directories = self.one('ls -d /system/x2d-un-retained-???????? 2>/dev/null; true').splitlines()
        require(len(directories) <= 8, '卸载保留目录过多，未清理')
        for directory in directories:
            m = re.fullmatch('/system/x2d-un-retained-([0-9a-f]{8})', directory)
            require(m is not None, '卸载保留目录格式不符')
            stage = '/data/local/tmp/x2d-un-'+m[1]
            reply = self.one('d='+stage+'; f=$d/cleanup.sh; if test ! -L $d && test -d $d && test ! -L $f && test -f $f; then echo FILE; else echo ABSENT; fi')
            require(reply == 'FILE', '卸载清理脚本不存在')
            reply = self.one('cd '+stage+' && (sh ./cleanup.sh "$PWD" >cleanup.log 2>&1; r=$?; echo $r; tail -n 10 cleanup.log)')
            lines = reply.splitlines()
            require(lines and lines[0] == '0' and 'RETAINED_PATCH_FILES_CLEANED' in lines, '卸载保留文件未清理完成；请确认手动关机开机，再诊断')

    def install(self):
        self.status('核对原厂状态与暂存目录')
        self.cleanup()
        self.data_cleanup('before')
        stage = '/data/local/tmp/x2d-rs1'
        self.ensure_stage(stage)
        idle = self.stage_idle()
        if idle == 'TRANSACTION':
            self.preserve_stage()
        else:
            require(idle == 'STACK_STAGE_IDLE', '暂存事务状态未确认')
        self.context.update(target='/data 可用空间',expected='可用空间大于32768 KiB（32 MiB）')
        reply = self.one(DATA_SPACE_COMMAND)
        self.log.append('空间读取结果：'+reply)
        free = parse_data_space(reply)
        self.log.append('STACK_DATA_FREE_KIB='+str(free))
        require(self.property('camera','exposure_status') == '0' and self.property('storage','storage_status') == '0' and self.property('error','active_error') == 'false', '相机非空闲或存在原厂错误，未上传安装文件')
        token = 'p-'+self.stock['archive_sha256'][:8]+'-usb'
        self.upload((self.root/'payload/install-payload.tar.gz').read_bytes(),stage,token,self.stock['archive_sha256'])
        require(self.stage_idle() == 'STACK_STAGE_IDLE', '上传期间暂存事务状态改变，未启动安装')
        self.one('cd '+stage+' && /system/xbin/busybox tar xzf '+token+'.gz')
        require(self.one('cd '+stage+' && test ! -L install.sh && test -r install.sh && sh -n install.sh && echo SCRIPT_READY') == 'SCRIPT_READY', '安装脚本缺失、不可读或语法错误')
        self.status('正在安装；请保持 USB 连接')
        self.one('cd '+stage+' && test ! -e started && (set -C; : > started) && nohup sh install.sh '+str(self.stock['feature_mask'])+' > install.log 2>&1 < /dev/null &')
        for _ in range(90):
            reply = self.one('cd '+stage+' && if test -e done; then cat done; elif test -e result; then cat result && tail -n 15 install.log; else tail -n 3 install.log; fi')
            phases={'RUNTIME_CHECKS':'检查机内运行库','SERVICES_STOPPING':'暂停服务','FILES_SWITCHING':'切换补丁文件','WAITING_SERVICES':'等待相机服务','SERVICES_READY':'相机服务已恢复'}
            reported=re.findall(r'INSTALL_STAGE=([A-Z_]+)',reply)
            if reported and reported[-1] in phases:
                self.status(phases[reported[-1]])
                self.progress('PATCH_PROGRESS=install|'+str(list(phases).index(reported[-1]))+'|5')
            if reply in ('RAW_STACK_BACKEND_INSTALLED_NO_CAPTURE','RAW_STACK_STOCK_FILES_INSTALLED_MANUAL_REBOOT_REQUIRED'):
                break
            if re.match(r'^\d+\s*\n', reply):
                self.log.append(protected(reply)); raise PatchError('相机安装事务未完成；请读取诊断，不重复启动')
            time.sleep(1)
        else:
            raise PatchError('没有取得安装完成回执；请诊断，未重发安装')
        self.progress('PATCH_PROGRESS=install|5|5')
        require(matches(self.collect_snapshot('verify'), view(self.stock)), '安装后的文件校验不符，未重复安装')
        self.data_cleanup('installed')
        return '安装文件检查完成。请正常关机再开机；需要查看运行状态请点“诊断”。再次点击“检测并安装”会重新执行卸载与安装流程。'

    def uninstall(self, snapshot, selected_route):
        if selected_route == 'Stock':
            self.cleanup()
            self.data_cleanup('uninstalled')
            return '已是原厂文件，无需重复卸载。'
        removal_route(snapshot, self.removal)
        stage = '/data/local/tmp/x2d-un-'+secrets.token_hex(4)
        self.status('暂存卸载事务')
        require(self.one('test ! -e '+stage+' && test ! -L '+stage+' && mkdir -m700 '+stage+' && echo UNINSTALL_STAGE_CREATED') == 'UNINSTALL_STAGE_CREATED', '卸载暂存创建未确认')
        self.upload((self.root/'uninstall/payload.tar.gz').read_bytes(), stage, 'p', self.removal['archive_sha256'])
        self.one('cd '+stage+' && /system/xbin/busybox tar xzf p.gz')
        require(self.one('cd '+stage+' && test ! -L uninstall.sh && test -r uninstall.sh && sh -n uninstall.sh && echo SCRIPT_READY') == 'SCRIPT_READY', '卸载脚本缺失、不可读或语法错误')
        self.status('正在卸载并还原原厂文件')
        require(self.one('cd '+stage+' && test ! -e started && (nohup /system/bin/sh -c \'sh ./uninstall.sh "$PWD" >log 2>&1\' >/dev/null 2>&1 </dev/null & echo UNINSTALL_DISPATCHED)') == 'UNINSTALL_DISPATCHED', '卸载派发未确认；没有重发')
        for _ in range(150):
            result = self.one('cd '+stage+' && if test -f result; then cat result; else echo RUNNING; fi')
            if re.fullmatch(r'\d+', result):
                self.log.append(protected(self.one('cd '+stage+' && tail -n 20 log')))
                require(result == '0', '卸载事务未完成；保留现场，没有重发')
                require(self.one('cd '+stage+' && test ! -L done && cat done') == 'PATCH_UNINSTALLED_MANUAL_REBOOT_REQUIRED', '卸载完成回执缺失')
                break
            require(result == 'RUNNING', '卸载状态回复无效')
            time.sleep(.3)
        else:
            raise PatchError('卸载等待超时；请诊断，不重复启动事务')
        expected = {x['path']: x['original'] for x in self.removal['targets']}
        self.progress('PATCH_PROGRESS=install|5|5')
        require(matches(self.collect_snapshot('verify'), expected), '卸载后的原厂文件校验失败')
        require(self.one('awk \'$2=="/system" && $4 ~ /(^|,)ro(,|$)/{ok=1} END{print ok ? "SYSTEM_READONLY" : "UNKNOWN"}\' /proc/mounts') == 'SYSTEM_READONLY', '卸载后系统只读状态未确认')
        self.data_cleanup('uninstalled')
        return '原厂文件已还原并核对。请正常关机后重新开机。'

    def property(self, service, prop):
        command = 's='+service+';p='+prop+';/bin/dbus-send --system --print-reply --reply-timeout=3000 --dest=com.hasselblad.$s /$s org.freedesktop.DBus.Properties.Get string:com.hasselblad.$s string:$p | awk \'/variant/{print $NF}\''
        return self.one(command)

    def verify_current(self):
        self.status('当前版本文件已核对；正在核对后台状态')
        menu = self.one('f=/tmp/x2d-native-menu-preload.status; if test -L $f; then echo LINK; elif test ! -e $f; then echo ABSENT; elif test ! -r $f; then echo UNREADABLE; else head -c 256 $f 2>/dev/null || echo READ_FAILED; fi')
        if menu == 'ABSENT':
            return '当前版本文件已完整核对，后台尚未启动。请手动关机开机后再点“检测并安装”；无需重装。'
        require(menu == 'MENU_AND_AFC_POINTERS_READY', '当前文件已核对；菜单运行状态未确认，请诊断，无需重装')
        if self.stock['capture_included']:
            encoded = self.one('f=/tmp/x2d-pixel-overclock/status.json; if test ! -L "$f" && test -f "$f"; then head -c 4096 "$f" | /system/xbin/busybox base64; else echo ABSENT; fi')
            try:
                worker = json.loads(base64.b64decode(encoded, validate=False))
            except Exception:
                raise PatchError('当前文件已核对；后台回复不完整，请诊断，无需重装') from None
            require(worker.get('protocol') == 3 and all(type(worker.get(k)) is bool for k in ('busy','ready','prepared','preparing')), '后台协议未确认，无需重装')
            if worker['busy'] or worker['preparing']:
                return '当前版本文件已核对。相机正在处理，请等任务结束后再检测；无需重装。'
            lock = self.one('f=/tmp/x2d-multishot-active; if test -L "$f"; then echo STACK_LOCK_UNSUPPORTED; elif test -d "$f"; then echo STACK_LOCK_HELD; elif test -e "$f"; then echo STACK_LOCK_UNSUPPORTED; else echo STACK_LOCK_RELEASED; fi')
            require(worker['ready'] and lock == ('STACK_LOCK_HELD' if worker['prepared'] else 'STACK_LOCK_RELEASED'), '后台与任务锁状态未确认，请诊断，无需重装')
            require(re.search(r'(^|,)ro(,|$)', self.one('awk \'$2=="/system"{print $4}\' /proc/mounts')), '系统只读状态未确认')
            require('rawStackUI=3 popup=1' in self.one('head -c 1024 /tmp/x2d-pixel-overclock/ui.txt'), '后台界面版本未确认')
        else:
            require(re.search(r'(^|,)ro(,|$)', self.one('awk \'$2=="/system"{print $4}\' /proc/mounts')), '系统只读状态未确认')
        require(self.property('error','active_error') == 'false', '原厂报告活动错误，请诊断，无需重装')
        require(self.property('camera','live_view_state') in ('0','1'), '取景状态未取得')
        if self.stock['capture_included']:
            endpoint = self.one('f=/tmp/x2d-pixel-overclock/memory.sock; if test -S "$f"; then if grep -Fq " $f" /proc/net/unix; then echo ACTIVE; else echo STALE; fi; elif test -e "$f"; then echo OTHER; else echo ABSENT; fi')
            self.log.append("显影通信端点："+endpoint+"；无监听残留会在下次模式准备时自动回收。")
        if self.stock.get('face_eye_included') or self.stock.get('af_speed_included'):
            for command, kind in [('pidof camera-service; cat /tmp/x2d-face-lock-backend.json','face'), ('pidof camera-service; head -c 600 /tmp/x2d-af-probe.json | sed \'s/,"events":.*$/}/\'','af')]:
                if not self.stock['face_eye_included' if kind=='face' else 'af_speed_included']:continue
                reply = self.one(command).split('\n', 1)
                require(len(reply) == 2 and reply[0].isdigit(), '后端进程状态缺失')
                try:
                    obj = json.loads(reply[1])
                except ValueError:
                    raise PatchError('后端状态格式不符') from None
                require(obj.get('protocol') == 1 and obj.get('ready') is True and obj.get('pid') == int(reply[0]), '后端就绪状态未确认：'+kind)
                if kind == 'face':
                    require(obj.get('eyeProtocol') == 1 and obj.get('ageFrames') == 10 and obj.get('ageError') == 0, '人脸人眼后端版本未确认')
                else:
                    require(obj.get('sender') is True, '对焦后端未就绪')
        if self.stock.get('face_eye_included'):
            for part in ('continuity','publication'):
                path='/tmp/x2d-face-'+part+'.json'
                reply=self.one('pidof camera-service; f='+path+'; if test -r "$f"; then cat "$f"; else echo ABSENT; fi').split('\n',1)
                require(len(reply)==2 and reply[0].isdigit() and reply[1]!='ABSENT', '跟随组件尚未启动：'+part+'；请完成正常重启，无需重复安装。')
                try:obj=json.loads(reply[1])
                except ValueError:raise PatchError('跟随组件状态格式不符：'+part) from None
                require(obj.get('protocol')==1 and obj.get('ready') is True and obj.get('pid')==int(reply[0]) and obj.get('reason')==0, '跟随组件未确认就绪：'+part+'；请读取诊断，无需重复安装。')
        return '已是当前补丁版本，文件与运行状态核对通过；无需重复安装。'

    def diagnose(self):
        self.status('正在只读采集诊断')
        try:
            self.log.append('当前息屏参数='+str(self.screen_get()))
        except Exception:
            self.log.append('当前息屏参数未取得；继续采集其他诊断')
        items = mac_diagnostics.pin_task(read(self.root/'diagnostic-commands.json'),self.one(mac_diagnostics.TASK_SELECTION_COMMAND))
        self.progress('PATCH_PROGRESS=diagnose|0|'+str(len(items)))
        replies = self.batch([item['command'] for item in items])
        self.progress('PATCH_PROGRESS=diagnose|'+str(len(items))+'|'+str(len(items)))
        converted = mac_diagnostics.convert(items,replies,protected)
        self.context.update({key:value for key,value in converted.items() if key != 'raw'})
        self.context.update(reason='只读现场采集完成；历史日志不证明本次安装成功或失败。',host_exit='0',
                            recovery='未确认。本报告只读状态和安装回执，不重新安装或恢复设置。')
        self.log.append(converted['raw'])
        return '只读诊断完成。历史回执不能证明属于本次安装。'

    def run(self, action):
        require(action in ('Install','Uninstall','Diagnose'), '未开放的操作')
        self.log = []
        self.context = dict(action=action,route='未识别',stage='初始化',command='未执行',target='未取得',expected='未取得',actual='未取得',
                            host_exit='未取得',camera_exit='未取得',state='未确认',readonly='未确认',recovery='未确认，不能据此重复安装',
                            transport='未打开 USB',time=datetime.now().astimezone().isoformat(timespec='seconds'))
        if action != 'Diagnose':
            self.first_failure = ''
        self.status('核对安装包完整性')
        try:
            self.check_package()
            if self.transport is None:
                require(self.library is not None, '缺少 USB 库')
                listing = subprocess.check_output(['/bin/ps','-axo','comm'], timeout=5).decode('utf-8', errors='replace')
                require(not any(re.search(r'(^|/)Phocus(?:\.app/|$)', line, re.I) for line in listing.splitlines()), '请先完全退出 Phocus，再点击操作')
            if action == 'Diagnose':
                return self.diagnose()
            self.check_usb_prompt()
            screen = self.screen_get()
            if action == 'Install':
                require(screen in (300,0,-1), '请将相机息屏设为五分钟或关闭，再点击检测并安装')
            snapshot = self.collect_snapshot()
            selected = removal_route(snapshot,self.removal) if action == 'Uninstall' else route(snapshot,self.stock,self.legacy,self.previous,self.removal)
            self.context['route'] = selected
            if action == 'Install' and self.pending:
                require(selected == 'Stock', '更新续接时原厂文件不符，未继续安装')
                if self.boot() == self.pending:
                    return '旧版已卸载。请先手动关机开机，再点“检测并安装”继续；本次未重复操作。'
            if action == 'Install' and selected == 'CurrentStock':
                result = self.verify_current(); self.pending = ''; return result
            self.lease_start()
            if action == 'Uninstall':
                result = self.uninstall(snapshot, selected); self.pending = ''; return result
            if selected == 'Stock':
                result = self.install(); self.pending = ''; return result
            before = self.boot()
            self.uninstall(snapshot, selected)
            # 必须在 finally 的息屏恢复也成功后才能提交续接状态。
            self.lease_stop()
            self.pending = before
            self.pending_mask = self.stock['feature_mask']
            return '旧版已卸载。保持客户端打开，手动关机开机后点“检测并安装”继续更新。'
        except Exception as exc:
            self.context.update(reason=protected(exc),host_exit='1')
            failure = '阶段：'+self.stage+'\n'+protected(exc)+'\n未自动重连、重发或重启；状态未确认时请诊断。'
            if not self.first_failure:
                self.first_failure = mac_diagnostics.format_context(self.context)
            self.log.append(failure)
            raise
        finally:
            if self.lease:
                try:
                    self.lease_stop()
                except Exception as exc:
                    message='本次文件事务可能已完成；息屏恢复未确认，请在相机设置中核对。不要重复安装。'
                    self.context.update(stage='恢复原自动息屏设置',target='/system screen_idle_timeout',reason=message+' '+protected(exc),
                                        host_exit='1',recovery='原息屏设置恢复未确认；没有重发文件事务')
                    if not self.first_failure:
                        self.first_failure=mac_diagnostics.format_context(self.context)
                    self.log.append('息屏恢复未确认：'+protected(exc))
                    raise PatchError(message) from exc

    def diagnostic_text(self):
        return 'X2D R5.8.52 · 固件 4.2.0\n'+mac_diagnostics.format_context(self.context,self.first_failure)+'\n\n本次原始输出（已脱敏）\n'+'\n\n'.join(self.log)
