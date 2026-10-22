"""只读分类卸载审计中的固定暂存目录；不读取照片或删除材料。"""
import argparse
import json
from pathlib import Path
import re
import sys
from audit_uninstalled_readonly import ROOT, OUTPUTS, verify_local_package


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('package', type=Path)
    ap.add_argument('audit', type=Path)
    ap.add_argument('output', type=Path)
    args = ap.parse_args()
    package, audit, output = (p.resolve() for p in (args.package, args.audit, args.output))
    assert Path.cwd().resolve() == ROOT and audit.is_relative_to(OUTPUTS)
    assert output.is_relative_to(OUTPUTS) and not output.exists()
    result = dict(identity=verify_local_package(package), read_only=True, entries=[])
    source = json.loads(audit.read_text(encoding='utf8'))
    assert source['package'] == str(package)
    rows = [x for x in source['checks'] if x['category'] in ('stage', 'blackbox')]
    sys.path.insert(0, str(package/'engine'))
    import windows_engine
    transport = windows_engine.Transport(package)
    def one(command):
        assert len(command.encode('ascii')) <= 231
        return transport.batch([command])[0].strip()
    try:
        for row in rows:
            assert row['reply'].endswith('AUDIT_END')
            for path in row['reply'].splitlines()[:-1]:
                assert re.fullmatch(r'/(?:blackbox|data/local/tmp)/x2d-[A-Za-z0-9_-]+', path)
                item = dict(path=path)
                result['entries'].append(item)
                item['type'] = one('d='+path+';test ! -L "$d" && stat -c "%F %a %u %h" "$d"')
                item['size_kib'] = one('du -sk '+path+' | cut -f1')
                item['extension_counts'] = one(
                    'find '+path+' -maxdepth 3 -type f | '
                    'awk -F. \'{if(NF>1)print $NF;else print "NO_EXTENSION"}\' | sort | uniq -c')
                if path.startswith('/data/local/tmp/x2d-tidy-'):
                    item['receipt'] = one('head -c 4096 '+path+'/final')
                elif path == '/data/local/tmp/x2d-bootstrap':
                    # 仅列已知控制回执的 basename；不读任何动态设备标识。
                    item['control_files'] = one(
                        'd='+path+';for p in "$d"/bootstrap*;do test ! -f "$p" || basename "$p";done')
    except Exception as exc:
        result['error_type'] = type(exc).__name__
    finally:
        output.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf8')
    print(json.dumps(result, ensure_ascii=False))


if __name__ == '__main__':
    main()
