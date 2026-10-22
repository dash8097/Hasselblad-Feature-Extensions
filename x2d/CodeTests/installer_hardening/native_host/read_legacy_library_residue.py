"""只读核对固定的十三个卸载遗漏；不扫描照片、不清理、不重试。"""
import argparse
import json
from pathlib import Path
import sys
from audit_uninstalled_readonly import ROOT, OUTPUTS, verify_local_package

NAMES = (
    'libx2d_afc_candidate.so', 'libx2d_afc_port_roi.so',
    'libx2d_native_menu.so.brdy.old',
    'libx2d_native_menu.so.cm-source-archive-retained',
    'libx2d_native_menu.so.rkey.old', 'libx2d_native_menu.so.rmdl.old',
    'libx2d_quarter_native.so.rcf1.old', 'libx2d_quarter_native.so.rcf2.old',
    'libx2d_quarter_native.so.rcf3.old', 'libx2d_quarter_native.so.rcf4.old',
    'libx2d_sensor_flash_r2.so', 'libx2d_sensor_flash_ui_r2.so',
    'libx2d_sensor_hotshoe_r2.so',
)


def main():
    p = argparse.ArgumentParser()
    p.add_argument('package', type=Path)
    p.add_argument('output', type=Path)
    a = p.parse_args()
    package, output = a.package.resolve(), a.output.resolve()
    assert Path.cwd().resolve() == ROOT
    assert output.is_relative_to(OUTPUTS) and not output.exists()
    result = dict(identity=verify_local_package(package), read_only=True, files=[])
    sys.path.insert(0, str(package / 'engine'))
    import windows_engine
    transport = windows_engine.Transport(package)
    try:
        for name in NAMES:
            path = '/system/lib64/' + name
            command = ('f=' + path + ';if test -L "$f";then echo LINK;'
                       'elif test -f "$f";then sha256sum "$f";'
                       'stat -c "%a %u %h %s" "$f";else echo ABSENT;fi')
            assert len(command.encode('ascii')) <= 231
            result['files'].append(dict(path=path, reply=transport.batch([command])[0].strip()))
    except Exception as exc:
        result['error_type'] = type(exc).__name__
    finally:
        output.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf8')
    print(json.dumps(result, ensure_ascii=False))


if __name__ == '__main__':
    main()
