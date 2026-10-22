"""固定6路文件通道的隔离ARM64 Linux实测；无设备、主机目录/网络透传。"""
from pathlib import Path
import hashlib,json,os,subprocess,sys
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[3]
sys.path.insert(0,str(HERE.parent/'camera_native'))
import checker_linux_env as linux
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 assert Path.cwd().resolve()==ROOT
 paths=[HERE/n for n in ('pixel_file_io.h','pixel_file_io.c','test_pixel_file_io.c','build_pixel_file_io_tests.py')]
 identity={p.name:sha(p) for p in paths};identity['compiler']=sha(linux.ZIG);identity['harness']=sha(Path(linux.__file__))
 out=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/native-ui/pixel-file-io'/hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()[:20]
 build=out/'private-build';build.mkdir(parents=True,exist_ok=False)
 for p in paths:(build/p.name).write_bytes(p.read_bytes())
 env=dict(os.environ,TEMP=str(build),TMP=str(build),ZIG_LOCAL_CACHE_DIR=str(build/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(build/'zig-global'))
 cmd=[linux.ZIG,'cc','-target','aarch64-linux-musl','-static','-std=c11','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-pthread','-s',build/'pixel_file_io.c',build/'test_pixel_file_io.c','-o',build/'init']
 r=subprocess.run(list(map(str,cmd)),env=env,capture_output=True,text=True,timeout=240);(out/'compiler.txt').write_text(r.stdout+r.stderr,encoding='utf8');assert not r.returncode,r.stdout+r.stderr
 (build/'initramfs.cpio').write_bytes(linux.cpio([('dev',b'',0o40755),('proc',b'',0o40755),('init',(build/'init').read_bytes(),0o100755)]))
 cmd=[linux.DEPS/'qemu/qemu-system-aarch64.exe','-machine','virt','-cpu','cortex-a53','-m','128','-smp','2','-nic','none','-display','none','-monitor','none','-serial','stdio','-no-reboot','-kernel',linux.DEPS/'vmlinuz-virt','-initrd',build/'initramfs.cpio','-append','console=ttyAMA0 rdinit=/init panic=-1 quiet']
 r=subprocess.run(list(map(str,cmd)),capture_output=True,text=True,timeout=60);(out/'console.txt').write_text(r.stdout+r.stderr,encoding='utf8');assert 'PIXEL_IO_PASSED' in r.stdout,r.stdout+r.stderr
 assert all(sha(p)==identity[p.name] for p in paths)
 (out/'report.json').write_text(json.dumps(dict(identity=identity,passed=True,author_tests=True,camera_operations=0,actual_arm_linux=True,bionic_executed=False,gui_integrated=False,stdout=r.stdout),ensure_ascii=False,indent=2),encoding='utf8');print(out);print(r.stdout)
if __name__=='__main__':main()
