"""隔离内核中的真实引闪进程退出／拒绝路径；冻结生产卸载源码。"""
import argparse,hashlib,json,os,subprocess,shutil
from pathlib import Path
import checker_linux_env as linux
HERE=Path(__file__).resolve().parent;ROOT=linux.ROOT
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser();p.add_argument('revision',nargs='?');p.add_argument('--live-source',action='store_true');a=p.parse_args();assert bool(a.revision)!=a.live_source
 base=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7'
 names=['native_uninstall.cpp','native_common.cpp','native_common.hpp','native_setup_tables.hpp','receipt_sha256.h']
 if a.live_source:
  inputs={n:HERE/n for n in names};inputs['receipt_sha256.h']=HERE.parents[1]/'pixel_shift_rgb/receipt_sha256.h'
 else:
  rev=base/'native-flow/revisions'/a.revision;proof=json.loads((rev/'build-proof.json').read_text(encoding='utf8'));inputs={n:rev/'private-build/source'/n for n in names}
  for n,p in inputs.items():assert sha(p)==proof['source_sha256'][n]
 assert sha(inputs['native_uninstall.cpp'])==sha(HERE/'native_uninstall.cpp')
 inputs['test_flash_uninstall.cpp']=HERE/'test_flash_uninstall.cpp';identity={n:sha(p) for n,p in inputs.items()};identity['builder']=sha(Path(__file__))
 out=base/'flash-uninstall-check'/hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()[:20];out.mkdir(parents=True,exist_ok=False)
 for n,p in inputs.items():shutil.copyfile(p,out/n)
 env=dict(os.environ,TEMP=str(out),TMP=str(out),ZIG_LOCAL_CACHE_DIR=str(out/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(out/'zig-global'))
 command=[str(linux.ZIG),'c++','-target','aarch64-linux-musl','-static','-std=c++17','-O2','-ffunction-sections','-fdata-sections',str(out/'test_flash_uninstall.cpp'),str(out/'native_common.cpp'),'-Wl,--gc-sections','-o',str(out/'init')]
 r=subprocess.run(command,env=env,capture_output=True,text=True,timeout=240);assert not r.returncode,r.stderr
 binary=(out/'init').read_bytes();entries=[(n,b'',0o40755) for n in ['dev','proc','system','system/bin','foreign']]+[(n,binary,0o100755) for n in ['init','foreign/x2d-esflash','system/bin/x2d-esflash']]
 (out/'initramfs.cpio').write_bytes(linux.cpio(entries))
 command=[str(linux.DEPS/'qemu/qemu-system-aarch64.exe'),'-machine','virt','-cpu','cortex-a53','-m','128','-smp','1','-nic','none','-display','none','-monitor','none','-serial','stdio','-no-reboot','-kernel',str(linux.DEPS/'vmlinuz-virt'),'-initrd',str(out/'initramfs.cpio'),'-append','console=ttyAMA0 rdinit=/init panic=-1 quiet']
 r=subprocess.run(command,capture_output=True,text=True,timeout=45);(out/'console.txt').write_text(r.stdout+r.stderr);print(r.stdout);baseline_rejected='FLASH_UNINSTALL_STALE_BASELINE_REJECTED' in r.stdout;passed='FLASH_UNINSTALL_PASSED' in r.stdout and 'FLASH_UNINSTALL_FAILED' not in r.stdout and baseline_rejected
 (out/'report.json').write_text(json.dumps(dict(identity=identity,passed=passed,baseline_stale_read_rejected=baseline_rejected,real_kernel=True,production_function=True,camera_requests=0),indent=2));assert passed;print(out)
if __name__=='__main__':main()
