"""原调度预算负例、新预算边界与隔离ARM Linux真实ppoll；不操作相机。"""
from pathlib import Path
import hashlib,json,os,subprocess,sys
ROOT=Path(__file__).resolve().parents[3];HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT/'x2d/CodeTests/installer_hardening/camera_native'))
import checker_linux_env as linux
def sha(b):return hashlib.sha256(b).hexdigest()
def main():
    assert Path.cwd().resolve()==ROOT
    files={n:(HERE/n).read_bytes() for n in ('native_wait_policy.h','test_native_wait.c')}
    identity={'sources':{n:sha(b) for n,b in files.items()},'builder':sha(Path(__file__).read_bytes())}
    out=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/esflash-wait-policy'/sha(json.dumps(identity,sort_keys=True).encode())[:20];out.mkdir(parents=True,exist_ok=False)
    env=dict(os.environ,TEMP=str(out),TMP=str(out),ZIG_LOCAL_CACHE_DIR=str(out/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(out/'zig-global'))
    for n,b in files.items():(out/n).write_bytes(b)
    def run(args,timeout=180):
        r=subprocess.run(list(map(str,args)),capture_output=True,text=True,env=env,timeout=timeout);assert r.returncode==0,r.stdout+r.stderr;return r
    flags=['cc','-O2','-Wall','-Wextra','-Werror','-std=c11']
    run([linux.ZIG,*flags,'-target','x86_64-windows-gnu',out/'test_native_wait.c','-o',out/'test.exe'])
    green=run([out/'test.exe'],10);assert 'NATIVE_WAIT_PASSED' in green.stdout
    baseline=out/'baseline';baseline.mkdir();(baseline/'test_native_wait.c').write_bytes(files['test_native_wait.c']);(baseline/'native_wait_policy.h').write_bytes(files['native_wait_policy.h'].replace(b'sampling?10000000ull:',b'sampling?1000000ull:'))
    run([linux.ZIG,*flags,'-target','x86_64-windows-gnu',baseline/'test_native_wait.c','-o',baseline/'test.exe'])
    red=subprocess.run([str(baseline/'test.exe')],capture_output=True,text=True,timeout=10);assert red.returncode==1 and 'NATIVE_WAIT_FAILED' in red.stdout
    run([linux.ZIG,*flags,'-D_GNU_SOURCE','-DX2D_WAIT_LINUX','-target','aarch64-linux-musl','-static',out/'test_native_wait.c','-o',out/'init'])
    (out/'initramfs.cpio').write_bytes(linux.cpio([('init',(out/'init').read_bytes(),0o100755)]))
    r=run([linux.DEPS/'qemu/qemu-system-aarch64.exe','-machine','virt','-cpu','cortex-a53','-m','128','-smp','1','-nic','none','-display','none','-monitor','none','-serial','stdio','-no-reboot','-kernel',linux.DEPS/'vmlinuz-virt','-initrd',out/'initramfs.cpio','-append','console=ttyAMA0 rdinit=/init panic=-1 quiet'],30)
    (out/'console.txt').write_text(r.stdout+r.stderr,encoding='utf8');passed='NATIVE_WAIT_PASSED' in r.stdout and 'kernel_poll=1' in r.stdout and 'NATIVE_WAIT_FAILED' not in r.stdout
    report=dict(identity=identity,passed=passed,baseline_budget_rejected=True,real_kernel_poll=True,host_stdout=green.stdout,camera_requests=0,hardware_timing_measured=False)
    (out/'report.json').write_text(json.dumps(report,indent=2),encoding='utf8');assert passed,r.stdout+r.stderr;print(out)
if __name__=='__main__':main()
