"""真实 ARM64 Linux 子进程管道验证；DBus 输出为替身，不连接相机。"""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
from album_stream_patch import patch
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
sys.path.insert(0,str(HERE.parent))
sys.path.insert(0,str(HERE.parent/'camera_native'))
import checker_linux_env as linux

def main():
    inputs={n:patch(n,(HERE.parent/'camera_native'/n).read_text(encoding='utf8')).encode()
            for n in ('native_common.hpp','native_common.cpp','native_publish.cpp')}
    inputs['test_album_stream.cpp']=(HERE/'test_album_stream.cpp').read_bytes()
    inputs['receipt_sha256.h']=(HERE.parents[1]/'pixel_shift_rgb/receipt_sha256.h').read_bytes()
    identity={n:hashlib.sha256(b).hexdigest() for n,b in inputs.items()}
    identity['patch']=hashlib.sha256((HERE/'album_stream_patch.py').read_bytes()).hexdigest()
    rev=hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()[:20]
    out=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/native-host/album-stream'/rev
    build=out/'private-build';build.mkdir(parents=True,exist_ok=True)
    for n,b in inputs.items():(build/n).write_bytes(b)
    env=dict(os.environ,TEMP=str(build),TMP=str(build),ZIG_LOCAL_CACHE_DIR=str(build/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(build/'zig-global'))
    common=[str(linux.ZIG),'c++','-target','aarch64-linux-musl','-std=c++17','-O2','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections']
    subprocess.run(common+['-static',str(build/'test_album_stream.cpp'),str(build/'native_common.cpp'),'-Wl,--gc-sections','-o',str(build/'init')],env=env,check=True,timeout=240)
    subprocess.run(common+['-c',str(build/'native_publish.cpp'),'-o',str(build/'publish.o')],env=env,check=True,timeout=240)
    initrd=build/'initramfs.cpio';initrd.write_bytes(linux.cpio([('dev',b'',0o40755),('init',(build/'init').read_bytes(),0o100755)]))
    args=[str(linux.DEPS/'qemu/qemu-system-aarch64.exe'),'-machine','virt','-cpu','cortex-a53','-m','256','-smp','1','-nic','none','-display','none','-monitor','none','-serial','stdio','-no-reboot','-kernel',str(linux.DEPS/'vmlinuz-virt'),'-initrd',str(initrd),'-append','console=ttyAMA0 rdinit=/init panic=-1 quiet']
    r=subprocess.run(args,capture_output=True,text=True,timeout=90)
    (out/'console.txt').write_text(r.stdout+r.stderr,encoding='utf8')
    assert 'ALBUM_STREAM_PASS' in r.stdout,r.stdout+r.stderr
    (out/'report.json').write_text(json.dumps(dict(identity=identity,passed=True,hardware_calls=0,limitations=['DBus output is a child-process fixture; camera acceptance pending']),indent=2)+'\n',encoding='utf8')
    print(out)
if __name__=='__main__':main()
