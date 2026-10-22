"""生产握手读写函数的 ARM64 Linux 离线测试；无相机、网络或主机共享。"""
from pathlib import Path
import hashlib,json,os,shutil,subprocess,sys
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[2]
NATIVE=ROOT/'x2d/CodeTests/installer_hardening/camera_native'
sys.path.insert(0,str(NATIVE))
import checker_linux_env as b
def main():
 assert Path.cwd().resolve()==ROOT
 paths=[HERE/'check_memory_gate.cpp',HERE/'memory_request_gate.h',NATIVE/'native_memory_request.hpp',NATIVE/'native_common.hpp',NATIVE/'native_common.cpp',ROOT/'x2d/CodeTests/pixel_shift_rgb/receipt_sha256.h']
 hashes={str(p.relative_to(ROOT)):b.sha(p) for p in paths}
 revision=hashlib.sha256(json.dumps(hashes,sort_keys=True).encode()).hexdigest()[:20]
 out=ROOT/'x2d/outputs/4.2.0/raw-stack/continuous-candidate/memory-gate-check'/revision;out.mkdir(parents=True,exist_ok=True)
 for p in paths:shutil.copyfile(p,out/p.name)
 env=dict(os.environ,TEMP=str(out),TMP=str(out),ZIG_LOCAL_CACHE_DIR=str(out/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(out/'zig-global'))
 cmd=[str(b.ZIG),'c++','-std=c++17','-O2','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-target','aarch64-linux-musl','-static','-I'+str(out),str(out/'check_memory_gate.cpp'),str(out/'native_common.cpp'),'-o',str(out/'init')]
 r=subprocess.run(cmd,env=env,capture_output=True,text=True,timeout=180);assert not r.returncode,r.stderr
 (out/'initramfs.cpio').write_bytes(b.cpio([('tmp',b'',0o40755),('init',(out/'init').read_bytes(),0o100755)]))
 qc=[str(b.DEPS/'qemu/qemu-system-aarch64.exe'),'-machine','virt','-cpu','cortex-a53','-m','128','-smp','1','-nic','none','-display','none','-monitor','none','-serial','stdio','-no-reboot','-kernel',str(b.DEPS/'vmlinuz-virt'),'-initrd',str(out/'initramfs.cpio'),'-append','console=ttyAMA0 rdinit=/init panic=-1 quiet']
 r=subprocess.run(qc,capture_output=True,text=True,timeout=60);(out/'console.txt').write_text(r.stdout+r.stderr)
 passed=r.returncode==0 and 'MEMORY_GATE_PASSED checks=20' in r.stdout
 (out/'report.json').write_text(json.dumps(dict(passed=passed,source_sha256=hashes,commands=[cmd,qc],hardware_tested=False),indent=2)+'\n');print(r.stdout);print(out);assert passed
if __name__=='__main__':main()
