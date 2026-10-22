"""卸载后独立只读核验。默认只生成命令计划；--execute 才使用指定完整包的传输。

不安装、清理、重启、唤醒、拍摄或读取照片。必须先完成 GUI 卸载、正常重启、
再次 GUI 卸载的收尾。保留的清理回执单独列示，不能误称字节级零残留。
"""
import argparse
import hashlib
import json
import re
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
HERE = Path(__file__).resolve().parent
OUTPUTS = ROOT / 'x2d/outputs'
DLL_MARKERS = r'libx2d_|/system/(bin/x2d-|x2d-un-retained-)'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def path_ok(value):
    return bool(re.fullmatch(r'/[A-Za-z0-9_./-]+', value)) and '..' not in value.split('/')


def file_probe(path, expected='ABSENT', category='component'):
    assert path_ok(path)
    command = ('f=' + path + ';if test -L "$f";then echo LINK;elif test -f "$f";'
               'then sha256sum "$f";elif test -e "$f";then echo OTHER;else echo ABSENT;fi')
    return dict(category=category, path=path, command=command,
                expected=expected if expected == 'ABSENT' else expected + '  ' + path)


def listing(parent, pattern, category, expectation='empty'):
    assert path_ok(parent) and re.fullmatch(r'[A-Za-z0-9_.*?-]+', pattern)
    # 不读取目录内文件，不列照片名；只有固定程序/暂存目录的顶级名称。
    command = ('d=' + parent + ';test -r "$d" && test -x "$d" || exit 4;'
               'for p in "$d"/' + pattern + ';do '
               'if test -e "$p" || test -L "$p";then echo "$p";fi;done;echo AUDIT_END')
    return dict(category=category, command=command, expectation=expectation)


def plan(package):
    metadata = json.loads((package / 'native/client-metadata.json').read_text(encoding='utf8'))
    assert metadata['schema'] == 'X2D_NATIVE_CLIENT_V1' and metadata['firmware'] == '4.2.0'
    profiles = metadata.get('profiles', {'default': metadata})
    targets = {}
    for profile in profiles.values():
        for item in profile['removal']['targets']:
            p, expected = item['path'], item['original']
            assert p.startswith('/system/') and path_ok(p)
            assert expected == 'ABSENT' or re.fullmatch('[0-9a-f]{64}', expected)
            assert p not in targets or targets[p] == expected
            targets[p] = expected
    checks = [file_probe(p, expected, 'original' if expected != 'ABSENT' else 'component')
              for p, expected in sorted(targets.items())]
    for p in (
        '/blackbox/x2d-shutter-audio.disable',
        '/blackbox/x2d-shutter-audio.disable.next',
        '/blackbox/x2d-shutter-audio.skip-once',
        '/blackbox/x2d-liveview-timeout.txt',
        '/tmp/x2d-multishot-active', '/tmp/x2d-data-cleanup-lock',
        '/tmp/x2d-pixel-overclock', '/tmp/x2d-raw-stack',
        '/system/bin/x2d-native-setup',
    ):
        checks.append(file_probe(p, category='setting-or-runtime'))
    checks.extend((
        listing('/system', 'x2d-un-retained-*', 'retained'),
        listing('/system/bin', 'x2d-*', 'program-directory'),
        listing('/system/etc', 'x2d-*', 'program-directory'),
        listing('/system/lib64', 'libx2d_*', 'library'),
        listing('/data/local/tmp', 'x2d-*', 'stage', 'classify-receipts'),
        listing('/data', 'cnl-*', 'legacy-program', 'review'),
        listing('/data', 'x2d-four-*', 'legacy-program', 'review'),
        listing('/blackbox', 'x2d-*', 'blackbox', 'review'),
        dict(category='system-mount',
             command='awk \'$2=="/system"{print $4}\' /proc/mounts', expectation='readonly'),
        dict(category='worker',
             command='pidof worker;echo AUDIT_END', expected='AUDIT_END'),
        dict(category='project-executable',
             command='for p in /proc/[0-9]*;do x=$(readlink "$p/exe");case "$x" in /system/bin/x2d-*) echo "$p";;esac;done;echo AUDIT_END',
             expected='AUDIT_END'),
    ))
    for c in checks:
        assert len(c['command'].encode('ascii')) <= 231, c['command']
    return checks


def evaluate(check, reply):
    if 'expected' in check:
        return 'PASS' if reply == check['expected'] else 'FAIL'
    kind = check['expectation']
    if kind == 'readonly':
        return 'PASS' if len(reply.splitlines()) == 1 and 'ro' in reply.split(',') else 'FAIL'
    if not reply.endswith('AUDIT_END'):
        return 'INCONCLUSIVE'
    paths = reply.splitlines()[:-1]
    if not paths:
        return 'PASS'
    if kind == 'empty':
        return 'FAIL'
    if kind == 'classify-receipts':
        receipt = re.compile(r'/data/local/tmp/(x2d-bootstrap|x2d-tidy-[0-9a-f]{8})')
        return 'RECEIPTS_REQUIRE_REVIEW' if all(receipt.fullmatch(p) for p in paths) else 'FAIL'
    return 'REVIEW'


def verify_local_package(package):
    """执行前绑定外部冻结交付清单；不信任包内自称的当前版本。"""
    report = package.parent / 'report.json'
    source = json.loads(report.read_text(encoding='utf8'))
    assert len(source['files']) < 20000
    for name, expected in source['files'].items():
        assert not Path(name).is_absolute() and '..' not in Path(name).parts
        p = package / name
        assert p.is_file() and not p.is_symlink() and sha(p) == expected, name
    return dict(delivery_report_sha256=sha(report),
                metadata_sha256=sha(package / 'native/client-metadata.json'),
                installer_sha256=sha(package / 'HasselbladTuningPatchInstaller.exe'),
                transport_sha256=sha(package / 'X2dTransport.dll'))


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--package', required=True, type=Path)
    ap.add_argument('--output', required=True, type=Path)
    ap.add_argument('--execute', action='store_true',
                    help='只由负责相机的主任务执行；会开启指定包的正常 USB 控制传输')
    args = ap.parse_args()
    assert Path.cwd().resolve() == ROOT
    package, output = args.package.resolve(), args.output.resolve()
    assert package.is_relative_to(ROOT) and output.is_relative_to(OUTPUTS)
    assert not output.exists(), '不要覆盖先前证据'
    checks = plan(package)
    identity = verify_local_package(package)
    record = dict(schema='X2D_UNINSTALL_READONLY_AUDIT_V1', package=str(package),
                  identity=identity, timestamp=time.time(), execution=args.execute,
                  checks=checks, camera_requests=0,
                  limits=['不读取照片或拍摄任务产物',
                          '回执目录存在时必须对照本次已封口回执，不能推定无残留',
                          '进程映射仅核对三个原厂服务；不声称读取受权限限制的全部进程'])
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(record, ensure_ascii=False, indent=2), encoding='utf8')
    if not args.execute:
        print('PLAN_ONLY camera_requests=0 checks=' + str(len(checks)))
        return
    assert sys.version_info[:2] == (3, 11)
    # 导入确切完整包的 pyd；不导入 package_ui（它会额外创建目录）。
    sys.path.insert(0, str(package / 'engine'))
    import windows_engine
    assert Path(windows_engine.__file__).resolve().is_relative_to(package / 'engine')
    transport = windows_engine.Transport(package)
    def one(command):
        assert len(command.encode('ascii')) <= 231
        record['camera_requests'] += 1
        values = transport.batch([command])
        assert len(values) == 1
        return values[0].strip()
    def save():
        output.write_text(json.dumps(record, ensure_ascii=False, indent=2), encoding='utf8')
    try:
        # 独立只读检查合批，减少重复 USB 握手；失败不重发、不补判未收到项。
        for offset in range(0, len(checks), 20):
            group = checks[offset:offset+20]
            record['camera_requests'] += len(group)
            replies = transport.batch([c['command'] for c in group])
            assert len(replies) == len(group)
            for c, reply in zip(group, replies):
                c['reply'] = reply.strip()
                c['verdict'] = evaluate(c, c['reply'])
            save()
            print('READONLY_CHECKS='+str(offset+len(group)), flush=True)
        # 不读取 cmdline 或 maps 内容，只返回匹配标志；绑定 starttime 防 PID 复用。
        services = []
        record['service_maps'] = services
        for name in ('camera-service', 'camera-gui', 'camera-storage'):
            entry = dict(service=name)
            services.append(entry)
            pids = one('pidof ' + name)
            if not re.fullmatch('[1-9][0-9]{0,8}', pids):
                entry['verdict'] = 'INCONCLUSIVE'
                save()
                continue
            entry['pid'] = pids
            stat_cmd = "awk '{print $22}' /proc/" + pids + '/stat'
            before = one(stat_cmd)
            command = ("grep -qE '" + DLL_MARKERS + "' /proc/" + pids + '/maps;echo MAP_RC=$?')
            result = one(command)
            after = one(stat_cmd)
            entry.update(start_before=before, start_after=after, match_result=result)
            entry['verdict'] = ('PASS' if result == 'MAP_RC=1' and before == after
                                and before.isdigit() else
                                'FAIL' if result == 'MAP_RC=0' else 'INCONCLUSIVE')
            save()
        verdicts = [c['verdict'] for c in checks] + [c['verdict'] for c in services]
        record['result'] = 'PASS_COMPONENTS_AND_STATE' if all(v == 'PASS' for v in verdicts) else 'REVIEW_REQUIRED'
    except Exception as exc:
        # 不输出 transport 原文；防止异常携带未经筛选的设备信息。绝不自动重试。
        record['result'] = 'INCOMPLETE_NO_RETRY'
        record['error_type'] = type(exc).__name__
    save()
    print(record['result'])
    print(output)


if __name__ == '__main__':
    main()
