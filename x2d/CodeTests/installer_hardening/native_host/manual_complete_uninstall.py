"""本次用户明确授权的历史实验/回执清理；不是安装或通用目录清理器。

先由实际安装包卸载并独立审计，再固定白名单归档/核对，最后可显式执行删除。
不接触 SSD 相册和任务成片，不重连、不自动重试。
"""
import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import re
import sys
from audit_uninstalled_readonly import ROOT, OUTPUTS, plan, verify_local_package

BLACKBOX = tuple('/blackbox/x2d-' + s for s in (
    'early-menu-stage', 'input-stage', 'menu-stage', 'native-stage',
    'original-menu-stage', 'preview-stage', 'private-read-abi-04-adb',
    'private-read-abi-05-adb', 'private-read-abi-06-adb', 'private-read-abi-07-adb',
    'shutter-stage', 'writer-clone-01-adb', 'writer-clone-02-adb',
    'writer-clone-03-adb', 'writer-clone-04-adb', 'writer-clone-05-adb',
    'writer-clone-06-adb'))


def allowed(path):
    return path in BLACKBOX or path == '/data/local/tmp/x2d-bootstrap' or bool(
        re.fullmatch(r'/data/local/tmp/x2d-tidy-[0-9a-f]{8}', path))


def durable(path, data):
    with path.open('xb') as stream:
        stream.write(data)
        stream.flush()
        os.fsync(stream.fileno())
    assert path.read_bytes() == data


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('package', type=Path)
    ap.add_argument('audit', type=Path)
    ap.add_argument('output', type=Path)
    ap.add_argument('--delete', action='store_true')
    args = ap.parse_args()
    package, audit, output = (p.resolve() for p in (args.package, args.audit, args.output))
    assert Path.cwd().resolve() == ROOT
    assert audit.is_relative_to(OUTPUTS) and output.is_relative_to(OUTPUTS)
    assert not output.exists()
    prior = json.loads(audit.read_text(encoding='utf8'))
    assert prior['package'] == str(package) and prior['execution']
    assert [c['command'] for c in prior['checks']] == [c['command'] for c in plan(package)]
    # 必须先确认原厂文件、运行引用、辅助程序和系统只读；只允许两个残留类别待清。
    assert all(c['verdict'] == 'PASS' for c in prior['checks']
               if c['category'] not in ('stage', 'blackbox'))
    assert len(prior['service_maps']) == 3
    assert all(c['verdict'] == 'PASS' for c in prior['service_maps'])
    paths = []
    for row in prior['checks']:
        if row['category'] in ('stage', 'blackbox'):
            assert row['reply'].endswith('AUDIT_END')
            paths.extend(row['reply'].splitlines()[:-1])
    assert paths and len(paths) <= 32 and len(paths) == len(set(paths))
    assert all(allowed(p) for p in paths)
    identity = verify_local_package(package)
    assert prior['identity'] == identity
    output.mkdir()
    result = dict(package=str(package), identity=identity, audit_sha256=hashlib.sha256(
        audit.read_bytes()).hexdigest(), delete_authorized=args.delete, entries=[])
    sys.path.insert(0, str(package/'engine'))
    import windows_engine
    transport = windows_engine.Transport(package)

    def one(command):
        assert len(command.encode('ascii')) <= 231, command
        reply = transport.batch([command])
        assert len(reply) == 1
        return reply[0].strip()

    def many(commands):
        assert all(len(c.encode('ascii')) <= 231 for c in commands)
        result = []
        for offset in range(0, len(commands), 50):
            group = commands[offset:offset+50]
            values = transport.batch(group)
            assert len(values) == len(group)
            result.extend(v.strip() for v in values)
        return result

    def query(path, body):
        assert allowed(path)
        return one('d='+path+';'+body)

    def inspect(path):
        bodies = [
            'test -d "$d" && test ! -L "$d" && readlink -f "$d"',
            'stat -c "%u:%d:%i:%Y:%Z" "$d"',
            """awk -v d="$d" '$2==d||index($2,d"/")==1{print "MOUNT"}' /proc/mounts""",
            'find "$d" -xdev ! -type f ! -type d -print;echo END',
            'find "$d" -xdev -type f -links +1 -print;echo END',
            'find "$d" -xdev -type f | wc -l',
            'find "$d" -xdev -type d -print;echo END',
        ]
        canonical, info, mounts, types, links, count, directories = many(
            ['d='+path+';'+body for body in bodies])
        assert canonical == path
        assert re.fullmatch(r'0:\d+:\d+:\d+:\d+', info)
        # 不跨挂载；不处理软链接、特殊文件、硬链接和不可读目录。
        assert mounts == '' and types == 'END' and links == 'END'
        assert count.isdigit() and int(count) <= 256
        rows = []
        pages = ['d='+path+';find "$d" -xdev -type f -exec sha256sum {} \\; | sort | sed -n \''+
                 str(offset+1)+','+str(offset+8)+'p\';echo END'
                 for offset in range(0, int(count), 8)]
        for tree in many(pages):
            assert tree.endswith('END')
            rows.extend(tree.splitlines()[:-1])
        assert len(rows) == int(count)
        for line in rows:
            assert re.fullmatch(r'[0-9a-f]{64}  '+re.escape(path)+r'/[A-Za-z0-9_./-]+', line)
            assert '..' not in line[66:].split('/')
        assert directories.endswith('END')
        dirs = directories.splitlines()[:-1]
        assert path in dirs and len(dirs) <= 64
        assert all(p == path or re.fullmatch(re.escape(path)+r'/[A-Za-z0-9_./-]+', p) for p in dirs)
        assert all('..' not in p.split('/') for p in dirs)
        return info, rows, dirs

    def unused(path):
        # 全局 maps 与三项已绑定原厂服务检查；不宣称读到了受限系统服务的 FD。
        # 手工清理针对已卸载的固定历史文件；受限服务的句柄状态未取得。
        # 不停止原厂服务，也不删除未知目录或递归删除。
        commands = ['d='+path+';grep -lF "$d/" /proc/[0-9]*/maps;echo MATCH=$?']
        expected = ['MATCH=1']
        for service in prior['service_maps']:
            pid = service['pid']
            commands.extend(['pidof '+service['service'],
                             "awk '{print $22}' /proc/"+pid+'/stat'])
            expected.extend([pid, service['start_after']])
            # ls 输出未知错误不能被当作不存在；仅容许检查时消失的临时 FD。
            command = ('d='+path+';p=/proc/'+pid+
                       ';ls -l $p/fd/* $p/exe $p/cwd 2>&1|awk -v d="$d" '
                       "'/ -> /{if(index($0,\" -> \"d))print \"USED\";next}"
                       '/No such file or directory/{next}{print "ERROR"}'
                       "';echo END")
            commands.append(command)
            expected.append('END')
        commands.extend(['pidof worker;echo END',
                         'test ! -e /tmp/x2d-data-cleanup-lock && test ! -e /tmp/x2d-multishot-active && echo IDLE'])
        expected.extend(['END', 'IDLE'])
        replies = many(commands)
        assert replies == expected, [(i, r) for i, r in enumerate(replies) if r != expected[i]]

    try:
        assert one('id -u') == '0'
        assert 'ro' in one("""awk '$2=="/system"{print $4}' /proc/mounts""").split(',')
        for service in prior['service_maps']:
            assert one('pidof '+service['service']) == service['pid']
            assert one("awk '{print $22}' /proc/"+service['pid']+'/stat') == service['start_after']
        assert one('test ! -e /tmp/x2d-data-cleanup-lock && test ! -e /tmp/x2d-multishot-active && echo IDLE') == 'IDLE'
        assert one('pidof worker;echo END') == 'END'
        for index, path in enumerate(paths):
            print(path, 'INSPECT', flush=True)
            before, rows, dirs = inspect(path)
            print(path, 'REFERENCES', flush=True)
            unused(path)
            entry = dict(path=path, directory_identity=before, files=rows, directories=dirs,
                         delete_intent=args.delete, removed=False)
            result['entries'].append(entry)
            file_ids = {}
            directory_ids = {}
            dir_replies = many(['readlink -f '+d+';stat -c %d:%i:%u '+d for d in dirs])
            for directory, reply in zip(dirs, dir_replies):
                canonical, value = reply.splitlines()
                assert canonical == directory
                directory_ids[directory] = value
                assert re.fullmatch(r'\d+:\d+:0', directory_ids[directory])
            values = many(['stat -c %d:%i:%h:%u:%s:%Y:%Z '+line[66:] for line in rows])
            for line, value in zip(rows, values):
                name = line[66:]
                file_ids[name] = value
                assert re.fullmatch(r'\d+:\d+:1:0:\d+:\d+:\d+', file_ids[name])
            entry.update(file_ids=file_ids, directory_ids=directory_ids)
            # 回执原文保存至电脑并落盘/回读，之后才允许删除机内副本。
            if path.startswith('/data/local/tmp/'):
                archive = output / ('receipts-'+str(index))
                archive.mkdir()
                for line in rows:
                    name = line[66:]
                    assert int(file_ids[name].split(':')[4]) <= 131072
                    raw = base64.b64decode(''.join(one(
                        '/system/xbin/busybox base64 '+name).split()), validate=True)
                    assert len(raw) <= 131072 and hashlib.sha256(raw).hexdigest() == line[:64]
                    durable(archive / name.rsplit('/', 1)[1], raw)
            durable(output / ('inventory-'+str(index)+'.json'),
                    json.dumps(entry, ensure_ascii=False, indent=2).encode('utf8'))
            assert inspect(path) == (before, rows, dirs)
            unused(path)
            if args.delete:
                # 逐个已登记文件复验 SHA 后 unlink；不递归删除，未知新增文件会阻止 rmdir。
                checks = []
                expected = []
                for directory in dirs:
                    checks.append('d='+directory+';readlink -f "$d";stat -c %d:%i:%u "$d"')
                    expected.append(directory+'\n'+directory_ids[directory])
                for line in rows:
                    name = line[66:]
                    checks.append('f='+name+';readlink -f "$f";stat -c %d:%i:%h:%u:%s:%Y:%Z "$f"')
                    expected.append(name+'\n'+file_ids[name])
                assert many(checks) == expected
                commands = ['f='+line[66:]+';test ! -L "$f"&&sha256sum "$f"|grep -q ^'+
                            line[:64]+'&&rm "$f"&&echo REMOVED' for line in rows]
                # 独立文件逐条执行，每小批保存结果；不因部分失败重发已完成命令。
                for offset in range(0, len(commands), 20):
                    durable(output / ('dispatch-'+str(index)+'-'+str(offset)+'.json'),
                            json.dumps(dict(files=rows[offset:offset+20],
                                            state='DISPATCH_INTENT_RESULT_NOT_YET_KNOWN')).encode('utf8'))
                    replies = many(commands[offset:offset+20])
                    durable(output / ('delete-'+str(index)+'-'+str(offset)+'.json'),
                            json.dumps(dict(files=rows[offset:offset+20], replies=replies)).encode('utf8'))
                    assert all(reply == 'REMOVED' for reply in replies)
                checks = ['f='+line[66:]+';test ! -e "$f" && test ! -L "$f" && echo ABSENT'
                          for line in rows]
                assert all(reply == 'ABSENT' for reply in many(checks))
                for directory in sorted(dirs, key=lambda p: (-p.count('/'), p)):
                    assert one('readlink -f '+directory) == directory
                    assert one('stat -c %d:%i:%u '+directory) == directory_ids[directory]
                    one('d='+directory+';test ! -L "$d" && rmdir "$d"')
                    assert one('d='+directory+';test ! -e "$d" && test ! -L "$d" && echo ABSENT') == 'ABSENT'
                entry['removed'] = True
            print(path, 'REMOVED' if entry['removed'] else 'ARCHIVED_NOT_DELETED', flush=True)
        result['result'] = 'REMOVED_ALL_LISTED' if args.delete else 'ARCHIVED_NOT_DELETED'
    except Exception as exc:
        result['result'] = 'STOPPED_NO_RETRY'
        result['error_type'] = type(exc).__name__
        raise
    finally:
        durable(output / 'result.json', json.dumps(result, ensure_ascii=False, indent=2).encode('utf8'))


if __name__ == '__main__':
    main()
