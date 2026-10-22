"""真实ARM Linux验证脉冲函数的临时FIFO/1与恢复；无相机、网络或物理寄存器。"""
from pathlib import Path
import hashlib,json,os,subprocess,sys
ROOT=Path(__file__).resolve().parents[3];HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT/'x2d/CodeTests/installer_hardening/camera_native'))
import checker_linux_env as linux
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 assert Path.cwd().resolve()==ROOT
 names=['native_hotshoe_resident.c','native_pulse_test_platform.h','test_native_pulse.c','test_native_pulse_kernel.c']
 ident={'sources':{n:sha(HERE/n) for n in names},'builder':sha(Path(__file__))}
 rev=hashlib.sha256(json.dumps(ident,sort_keys=True).encode()).hexdigest()[:20]
 out=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/esflash-pulse-kernel'/rev;out.mkdir(parents=True,exist_ok=False)
 for n in names:(out/n).write_bytes((HERE/n).read_bytes())
 env=dict(os.environ,TEMP=str(out),TMP=str(out),ZIG_LOCAL_CACHE_DIR=str(out/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(out/'zig-global'))
 cmd=[str(linux.ZIG),'cc','-target','aarch64-linux-musl','-O2','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-static','-s',str(out/'test_native_pulse_kernel.c'),'-o',str(out/'init')]
 r=subprocess.run(cmd,capture_output=True,text=True,env=env,timeout=180);assert r.returncode==0,r.stdout+r.stderr
 (out/'initramfs.cpio').write_bytes(linux.cpio([('dev',b'',0o40755),('init',(out/'init').read_bytes(),0o100755)]))
 run=[str(linux.DEPS/'qemu/qemu-system-aarch64.exe'),'-machine','virt','-cpu','cortex-a53','-m','256','-smp','2','-nic','none','-display','none','-monitor','none','-serial','stdio','-no-reboot','-kernel',str(linux.DEPS/'vmlinuz-virt'),'-initrd',str(out/'initramfs.cpio'),'-append','console=ttyAMA0 rdinit=/init panic=-1 quiet']
 r=subprocess.run(run,capture_output=True,text=True,timeout=90);(out/'console.txt').write_text(r.stdout+r.stderr,encoding='utf8')
 passed=r.returncode==0 and 'NATIVE_PULSE_KERNEL_SCHED_PASSED cycles=64' in r.stdout
 for n,h in ident['sources'].items():assert sha(HERE/n)==h
 report={'passed':passed,'identity':ident,'real_kernel_scheduler':True,'hardware_mocked':True,'camera_operations':0,'credentials_or_policy_rules_modified':False,'build_command':cmd,'run_command':run,'limitations':['寄存器和时钟为替身；未测物理脉宽、实机调度延迟或相机上的调度接口访问结果。']}
 (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8');print(r.stdout);print(out);assert passed
if __name__=='__main__':main()
