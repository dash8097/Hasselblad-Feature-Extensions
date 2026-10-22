"""固定模块身份与worker：真实ARM Linux映射/文件，地址为显式测试映射。"""
from pathlib import Path
import hashlib,json,os,subprocess,sys
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[3]
sys.path.insert(0,str(HERE.parent/'camera_native'))
import checker_linux_env as linux
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 assert Path.cwd().resolve()==ROOT
 paths=[HERE/n for n in ('loaded_module_identity.h','loaded_module_identity.c','ui_receipt.h','ui_receipt.c','ui_receipt_worker.c','test_loaded_module_identity.c','build_loaded_identity_tests.py')]+[ROOT/'x2d/CodeTests/pixel_shift_rgb/receipt_sha256.h']
 identity={p.name:sha(p) for p in paths};identity['compiler']=sha(linux.ZIG);identity['harness']=sha(Path(linux.__file__))
 out=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/native-ui/loaded-module-identity'/hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()[:20];b=out/'private-build';b.mkdir(parents=True,exist_ok=False)
 for p in paths:(b/p.name).write_bytes(p.read_bytes())
 env=dict(os.environ,TEMP=str(b),TMP=str(b),ZIG_LOCAL_CACHE_DIR=str(b/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(b/'zig-global'))
 cmd=[linux.ZIG,'cc','-target','aarch64-linux-musl','-std=c11','-static','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-pthread','-DX2D_UI_VERIFY_LOADED=1','-DX2D_IDENTITY_TEST=1',*[b/p.name for p in paths if p.suffix=='.c'],'-o',b/'init']
 r=subprocess.run(list(map(str,cmd)),env=env,capture_output=True,text=True,timeout=240);(out/'build.log').write_text(r.stdout+r.stderr,encoding='utf8');assert not r.returncode,r.stdout+r.stderr
 (b/'init.cpio').write_bytes(linux.cpio([('dev',b'',0o40755),('proc',b'',0o40755),('init',(b/'init').read_bytes(),0o100755)]))
 cmd=[linux.DEPS/'qemu/qemu-system-aarch64.exe','-machine','virt','-cpu','cortex-a53','-m','128','-smp','2','-nic','none','-display','none','-monitor','none','-serial','stdio','-no-reboot','-kernel',linux.DEPS/'vmlinuz-virt','-initrd',b/'init.cpio','-append','console=ttyAMA0 rdinit=/init panic=-1 quiet']
 r=subprocess.run(list(map(str,cmd)),capture_output=True,text=True,timeout=60);(out/'run.log').write_text(r.stdout+r.stderr,encoding='utf8');assert not r.returncode and 'LOADED_IDENTITY_PASS' in r.stdout,r.stdout+r.stderr
 assert all(sha(p)==identity[p.name] for p in paths)
 (out/'report.json').write_text(json.dumps(dict(identity=identity,passed=True,real_ARM_Linux_maps_files_worker=True,mapped_code_address_is_fixture=True,in_process_memory_attestation=False,hardware_operations=0,stdout=r.stdout),indent=2),encoding='utf8');print(out);print(r.stdout)
if __name__=='__main__':main()
