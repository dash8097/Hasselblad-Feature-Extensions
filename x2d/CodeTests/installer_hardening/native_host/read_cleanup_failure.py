"""只读固定卸载清理回执与程序残留目录；不读取照片、不重试事务。"""
import argparse
import json
from pathlib import Path
import re
import sys
from assemble_native_client import ROOT, R7


def main():
    p=argparse.ArgumentParser()
    p.add_argument('package',type=Path)
    p.add_argument('transaction')
    p.add_argument('output',type=Path)
    a=p.parse_args()
    package,out=a.package.resolve(),a.output.resolve()
    assert Path.cwd().resolve()==ROOT and package.is_relative_to(R7)
    assert out.is_relative_to(R7) and not out.exists()
    assert re.fullmatch('[0-9a-f]{8}',a.transaction)
    sys.path.insert(0,str(package/'engine'))
    import windows_engine
    w=windows_engine.Transport(package)
    stage='/data/local/tmp/x2d-tidy-'+a.transaction
    queries={
        'cleanup_receipt':'cat '+stage+'/final',
        'data_program_stages':'for p in /data/local/tmp/x2d-*;do test ! -e "$p" || ls -ld "$p";done;true',
        'system_retained':'ls -d /system/x2d-un-retained-* 2>/dev/null;true',
        'previous_setup_hashes':'sha256sum /system/bin/x2d-native-setup.previous.* 2>/dev/null;true',
        'tmp_runtime':'for p in /tmp/x2d-*;do test ! -e "$p" || ls -ld "$p";done;true',
        'blackbox_generated':'ls -ld /blackbox/x2d-* 2>/dev/null;true',
        'mount':'awk \'$2=="/system"{print $4}\' /proc/mounts',
        'exposure':"/bin/dbus-send --system --print-reply --dest=com.hasselblad.camera /camera org.freedesktop.DBus.Properties.Get string:com.hasselblad.camera string:exposure_status | tail -n 1",
        'storage':"/bin/dbus-send --system --print-reply --dest=com.hasselblad.storage /storage org.freedesktop.DBus.Properties.Get string:com.hasselblad.storage string:storage_status | tail -n 1",
        'error':"/bin/dbus-send --system --print-reply --dest=com.hasselblad.error /error org.freedesktop.DBus.Properties.Get string:com.hasselblad.error string:active_error | tail -n 1",
    }
    record=dict(package=str(package),read_only=True,values={})
    try:
        for name,command in queries.items():
            assert len(command.encode('ascii'))<=231
            record['values'][name]=w.batch([command])[0].strip()
    finally:
        out.write_text(json.dumps(record,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps(record['values'],ensure_ascii=False))


if __name__=='__main__':main()
