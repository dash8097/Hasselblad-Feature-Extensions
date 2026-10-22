"""隔离ARM64 Linux中的真实loopback服务测试；无主机网络、设备或目录透传。"""
from pathlib import Path
import hashlib,json,os,shutil,subprocess,sys
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[3]
sys.path.insert(0,str(HERE.parent/'camera_native'))
import checker_linux_env as linux
assert Path.cwd().resolve()==ROOT
names=('esflash_client.c','esflash_client.h','esflash_protocol.h','test_esflash_client.c','test_esflash_client_linux.py')
hashes={n:hashlib.sha256((HERE/n).read_bytes()).hexdigest() for n in names}
revision=hashlib.sha256(json.dumps(hashes,sort_keys=True).encode()).hexdigest()[:20]
out=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/native-ui/esflash-client'/revision
assert not out.exists(),'不覆盖原测试证据'
build=out/'private-build';build.mkdir(parents=True)
for name in names:shutil.copyfile(HERE/name,build/name)
env=dict(os.environ,TEMP=str(build),TMP=str(build),ZIG_LOCAL_CACHE_DIR=str(build/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(build/'zig-global'))
cmd=[linux.ZIG,'cc','-target','aarch64-linux-musl','-static','-Oz','-Wall','-Wextra','-Werror','-s',build/'esflash_client.c',build/'test_esflash_client.c','-o',build/'init']
r=subprocess.run([str(a) for a in cmd],env=env,capture_output=True,text=True,timeout=240)
(out/'compiler.txt').write_text(r.stdout+r.stderr,encoding='utf8');assert not r.returncode,r.stdout+r.stderr
(build/'initramfs.cpio').write_bytes(linux.cpio([('dev',b'',0o40755),('proc',b'',0o40755),('init',(build/'init').read_bytes(),0o100755)]))
cmd=[linux.DEPS/'qemu/qemu-system-aarch64.exe','-machine','virt','-cpu','cortex-a53','-m','128','-smp','1','-nic','none','-display','none','-monitor','none','-serial','stdio','-no-reboot','-kernel',linux.DEPS/'vmlinuz-virt','-initrd',build/'initramfs.cpio','-append','console=ttyAMA0 rdinit=/init panic=-1 quiet']
r=subprocess.run([str(a) for a in cmd],capture_output=True,text=True,timeout=45)
(out/'console.txt').write_text(r.stdout+r.stderr,encoding='utf8');assert 'ESFLASH_PASSED' in r.stdout,r.stdout+r.stderr
(out/'report.json').write_text(json.dumps(dict(passed=True,source_sha256=hashes,author_tests=True,independent_approval=False,camera_requests=0,actual_linux_datagram=True,scope='实际 ARM64 引闪客户端及隔离 Unix socket 替身；不是相机实测'),ensure_ascii=False,indent=2),encoding='utf8')
print(r.stdout);print(out)
