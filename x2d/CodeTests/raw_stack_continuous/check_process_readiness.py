"""ARM Linux 真 procfs；仅 readlink 权限错误注入，验证生产进程枚举。"""
from pathlib import Path
import hashlib,json,os,subprocess,sys
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[2]
NATIVE=ROOT/'x2d/CodeTests/installer_hardening/camera_native'
sys.path.insert(0,str(NATIVE))
import checker_linux_env as linux
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 assert Path.cwd().resolve()==ROOT
 paths=[NATIVE/'native_common.cpp',NATIVE/'native_common.hpp',NATIVE.parent/'native_ui/arm_bridge/receipt_sha256.h',HERE/'check_process_readiness.cpp',Path(__file__)]
 identity={str(p.relative_to(ROOT)):sha(p) for p in paths}
 out=ROOT/'x2d/outputs/4.2.0/raw-stack/continuous-candidate/process-check'/sha_bytes(json.dumps(identity,sort_keys=True).encode())[:20]
 b=out/'private-build';b.mkdir(parents=True,exist_ok=False)
 for p in paths:(b/p.name).write_bytes(p.read_bytes())
 env=dict(os.environ,TEMP=str(b),TMP=str(b),ZIG_LOCAL_CACHE_DIR=str(b/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(b/'zig-global'))
 cmd=[linux.ZIG,'c++','-target','aarch64-linux-musl','-std=c++17','-static','-O2','-ffunction-sections','-fdata-sections','-Wl,--gc-sections',b/'check_process_readiness.cpp','-o',b/'init']
 r=subprocess.run(list(map(str,cmd)),env=env,capture_output=True,text=True,timeout=240);(out/'build.log').write_text(r.stdout+r.stderr,encoding='utf8');assert not r.returncode,r.stdout+r.stderr
 (b/'init.cpio').write_bytes(linux.cpio([('dev',b'',0o40755),('proc',b'',0o40755),('init',(b/'init').read_bytes(),0o100755)]))
 cmd=[linux.DEPS/'qemu/qemu-system-aarch64.exe','-machine','virt','-cpu','cortex-a53','-m','128','-smp','1','-nic','none','-display','none','-monitor','none','-serial','stdio','-no-reboot','-kernel',linux.DEPS/'vmlinuz-virt','-initrd',b/'init.cpio','-append','console=ttyAMA0 rdinit=/init panic=-1 quiet']
 r=subprocess.run(list(map(str,cmd)),capture_output=True,text=True,timeout=60);(out/'console.txt').write_text(r.stdout+r.stderr,encoding='utf8');assert not r.returncode and 'PROCESS_READINESS_PASS' in r.stdout,r.stdout+r.stderr
 assert all(sha(ROOT/n)==v for n,v in identity.items())
 (out/'report.json').write_text(json.dumps(dict(passed=True,source_sha256=identity,real_ARM_Linux_procfs=True,injected='exe readlink errno only',camera_requests=0,stdout=r.stdout),indent=2),encoding='utf8');print(out);print(r.stdout)
def sha_bytes(b):return hashlib.sha256(b).hexdigest()
if __name__=='__main__':main()
