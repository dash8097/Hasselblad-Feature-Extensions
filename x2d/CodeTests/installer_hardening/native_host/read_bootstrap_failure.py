"""只读指定引导失败现场，不重试提交、不改变设置或读取照片。"""
import argparse
import base64
import json
from pathlib import Path
import re
import sys
from assemble_native_client import ROOT, R7, encoded


def main(package, out, attempt, digest):
    assert Path.cwd().resolve()==ROOT
    package,out=package.resolve(),out.resolve()
    assert package.is_relative_to(R7) and out.is_relative_to(R7) and not out.exists()
    assert re.fullmatch('[0-9a-f]{16}',attempt) and re.fullmatch('[0-9a-f]{64}',digest)
    sys.path.insert(0,str(package/'engine'))
    import windows_engine
    w=windows_engine.Transport(package)
    data={}
    queries={
        'mount':"awk '$2==\"/system\"{print $4}' /proc/mounts",
        'seed_sha':'sha256sum /system/bin/x2d-'+digest[:8],
        'setup_sha':'sha256sum /system/bin/x2d-native-setup',
        'receipt':'/system/bin/x2d-'+digest[:8]+' bootstrap-read '+attempt,
        'exposure':"s=camera;p=exposure_status;/bin/dbus-send --system --print-reply --reply-timeout=3000 --dest=com.hasselblad.$s /$s org.freedesktop.DBus.Properties.Get string:com.hasselblad.$s string:$p | awk '/variant/{print $NF}'",
        'storage':"s=storage;p=storage_status;/bin/dbus-send --system --print-reply --reply-timeout=3000 --dest=com.hasselblad.$s /$s org.freedesktop.DBus.Properties.Get string:com.hasselblad.$s string:$p | awk '/variant/{print $NF}'",
    }
    try:
        for name,cmd in queries.items():
            assert len(cmd.encode('ascii'))<=231
            data[name]=w.batch([cmd])[0].strip()
            if name=='receipt':
                receipt=json.loads(data[name])
                if receipt['state']=='PRESENT':
                    data['decoded_receipt']=base64.b64decode(receipt['data'],validate=True).decode('ascii')
    finally:
        out.write_bytes(encoded(dict(attempt=attempt,read_only=True,values=data)))
    print(json.dumps(data,ensure_ascii=False))


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('package',type=Path);p.add_argument('out',type=Path)
    p.add_argument('attempt');p.add_argument('digest')
    a=p.parse_args();main(a.package,a.out,a.attempt,a.digest)
