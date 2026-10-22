"""精确原生worker+固定路径C runner替身；无相机与原厂程序。"""
import json,os,shutil,subprocess,argparse
from pathlib import Path
import checker_linux_env as b
def main():
    assert Path.cwd().resolve()==b.ROOT
    ap=argparse.ArgumentParser();ap.add_argument('--revision',default='1534d06106c852ffa42d');ap.add_argument('--sha',default='6dc855a5b9973e886dd7e6830d2b3c226a7593d11c4c4990bef95f61dfb8b93e');args=ap.parse_args()
    assert len(args.revision)==20 and all(c in '0123456789abcdef' for c in args.revision)
    root=b.ROOT/'x2d/outputs/4.2.0/installer-hardening-r7';rev=root/'native-worker/revisions'/args.revision;elf=rev/'artifacts/worker'
    expected=args.sha;assert b.sha(elf)==expected
    source=b.HERE/'checker_native_worker_init.c';out=root/'checker-ui'/('native-worker-'+expected[:8]+'-'+b.sha(source)[:8]);assert not (out/'report.json').exists();out.mkdir(parents=True,exist_ok=True)
    for src,dest in [(source,out/source.name),(elf,out/'worker'),(rev/'build-proof.json',out/'build-proof.json')]:shutil.copyfile(src,dest)
    env=dict(os.environ,TEMP=str(out),TMP=str(out),ZIG_LOCAL_CACHE_DIR=str(out/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(out/'zig-global'))
    cmd=[str(b.ZIG),'cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-target','aarch64-linux-musl','-static','-s',str(out/source.name),'-o',str(out/'init')]
    r=subprocess.run(cmd,env=env,capture_output=True,text=True,timeout=180);assert r.returncode==0,r.stderr
    entries=[(n,b'',0o40755) for n in ['dev','proc','tmp','mnt','system','system/bin']]+[('init',(out/'init').read_bytes(),0o100755),('system/bin/x2d-native-flow',(out/'init').read_bytes(),0o100755),('worker',elf.read_bytes(),0o100755)]
    (out/'initramfs.cpio').write_bytes(b.cpio(entries));cmd=[str(b.DEPS/'qemu/qemu-system-aarch64.exe'),'-machine','virt','-cpu','cortex-a53','-m','512','-smp','2','-nic','none','-display','none','-monitor','none','-serial','stdio','-no-reboot','-kernel',str(b.DEPS/'vmlinuz-virt'),'-initrd',str(out/'initramfs.cpio'),'-append','console=ttyAMA0 rdinit=/init panic=-1 quiet']
    r=subprocess.run(cmd,capture_output=True,text=True,timeout=240);(out/'console.txt').write_text(r.stdout+r.stderr,encoding='utf-8')
    passed=r.returncode==0 and 'NATIVE_WORKER_ALL_PASSED' in r.stdout and 'WORKER_CHECK_FAIL' not in r.stdout
    report=dict(passed=passed,artifact_sha256=expected,source_sha256=b.sha(source),command=cmd,exact_worker=True,runner='自造C替身',actual_Qt_UI=False,real_camera=False,network=False,usb=False,host_shared_directory=False,limits=['非完整相机工作流','不证明原厂连拍/存储忙/曝光/显影恢复','protocol经文件驱动未点击Qt菜单'])
    (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');print(r.stdout[-20000:]);print(out);assert passed
if __name__=='__main__':main()
