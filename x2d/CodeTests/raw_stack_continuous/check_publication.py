"""独立发布/释放凭据回归，编译冻结 stack 源及真实 common，未替换文件函数。"""
import json,os,shutil,subprocess,argparse
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"installer_hardening/camera_native"))
import checker_linux_env as b
import build_flow
def main():
    assert Path.cwd().resolve()==b.ROOT
    here=Path(__file__).resolve().parent
    names=['native_stack_runner.cpp','native_stack_runner.hpp','native_common.cpp','native_common.hpp','native_process.hpp','native_process.cpp','receipt_sha256.h','camera_native.h','native_runtime_tables.hpp','checker_stack_publication.cpp']
    source=lambda n: b.ROOT/"x2d/CodeTests/pixel_shift_rgb/receipt_sha256.h" if n=="receipt_sha256.h" else here/n if (here/n).is_file() else build_flow.input_path(n)
    import hashlib
    hashes={n:b.sha(source(n)) for n in names}
    identity=hashlib.sha256(json.dumps(hashes,sort_keys=True).encode()).hexdigest()[:20]
    out=b.ROOT/'x2d/outputs/4.2.0/raw-stack/continuous-candidate/publication-check'/identity
    out.mkdir(parents=True,exist_ok=True)
    for n in names:shutil.copyfile(source(n),out/n)
    expected=None
    env=dict(os.environ,TEMP=str(out),TMP=str(out),ZIG_LOCAL_CACHE_DIR=str(out/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(out/'zig-global'))
    cmd=[str(b.ZIG),'c++','-std=c++17','-O2','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-target','aarch64-linux-musl','-static','-s','-I'+str(out),str(out/'checker_stack_publication.cpp'),str(out/'native_common.cpp'),str(out/'native_process.cpp'),'-o',str(out/'init')]
    r=subprocess.run(cmd,env=env,capture_output=True,text=True,timeout=240);(out/'build.txt').write_text(r.stdout+r.stderr,encoding='utf-8');assert not r.returncode,r.stderr
    entries=[(n,b'',0o40755) for n in ['dev','proc','tmp','mnt']]+[('init',(out/'init').read_bytes(),0o100755)];(out/'initramfs.cpio').write_bytes(b.cpio(entries))
    qc=[str(b.DEPS/'qemu/qemu-system-aarch64.exe'),'-machine','virt','-cpu','cortex-a53','-m','512','-smp','2','-nic','none','-display','none','-monitor','none','-serial','stdio','-no-reboot','-kernel',str(b.DEPS/'vmlinuz-virt'),'-initrd',str(out/'initramfs.cpio'),'-append','console=ttyAMA0 rdinit=/init panic=-1 quiet']
    r=subprocess.run(qc,capture_output=True,text=True,timeout=90);(out/'console.txt').write_text(r.stdout+r.stderr,encoding='utf-8');passed=r.returncode==0 and 'STACK_PUBLICATION_ALL_PASSED cases=23' in r.stdout
    report=dict(passed=passed,production_artifact_reference=expected,source_sha256={n:b.sha(out/n) for n in names+['checker_stack_publication.cpp']},tested_elf_sha256=b.sha(out/'init'),actual_common_file_functions=True,actual_current_stack_functions=True,independently_authored_cases=False,cases=23,entire_runner_executed=False,production_exact_executed=False,publication_system='actual Linux rename on synthetic ramfs fixture, not exFAT',commands=[cmd,qc],real_camera=False,usb=False,network=False,host_shared_directory=False)
    (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');print(r.stdout);print(out);assert passed
if __name__=='__main__':main()
