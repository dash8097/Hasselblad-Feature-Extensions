"""精确拒绝路径+冻结完整memory runner状态机的叶子替身测试。"""
import json,os,shutil,subprocess,hashlib,argparse,re
from pathlib import Path
import checker_linux_env as b
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--impostor',action='store_true');parser.add_argument('--revision',default='2560b5c3fba3b6cb2931');parser.add_argument('--sha',default='9de45575175b58ce80a1aa0bcffdb77c629b1c95624a7761e4d8285f4970a693');parser.add_argument('--runtime-root',type=Path);args=parser.parse_args()
    assert Path.cwd().resolve()==b.ROOT
    root=b.ROOT/'x2d/outputs/4.2.0/installer-hardening-r7';rev=(args.runtime_root or root/'native-flow')/'revisions'/args.revision;src=rev/'private-build/source';elf=rev/'artifacts/x2d-native-flow';expected=args.sha;assert b.sha(elf)==expected
    names=['checker_memory_full_adapter.cpp','checker_memory_full_leaf.c','checker_memory_full_init.c'];ident=hashlib.sha256((''.join(b.sha(b.HERE/n) for n in names)+b.sha(Path(__file__))+str(args.impostor)).encode()).hexdigest()[:12];out=root/'checker-ui'/('memory-full-'+expected[:8]+'-'+ident);assert not (out/'report.json').exists();out.mkdir(parents=True,exist_ok=True)
    proof=json.loads((rev/'build-proof.json').read_text(encoding='utf-8'));shutil.copyfile(rev/'build-proof.json',out/'build-proof.json');shutil.copyfile(elf,out/'exact')
    for n in names:shutil.copyfile(b.HERE/n,out/n)
    frozen=['native_memory_runner.cpp','native_memory_runner.hpp','native_memory_request.hpp','native_common.cpp','native_common.hpp','receipt_sha256.h','camera_native.c','camera_native.h']
    if '#include "native_runtime_tables.hpp"' in (src/'native_memory_runner.cpp').read_text(encoding='utf-8'):frozen.append('native_runtime_tables.hpp')
    for n in frozen:assert b.sha(src/n)==proof['source_sha256'][n];shutil.copyfile(src/n,out/n)
    env=dict(os.environ,TEMP=str(out),TMP=str(out),ZIG_LOCAL_CACHE_DIR=str(out/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(out/'zig-global'))
    commands=[]
    def run(cmd):
        commands.append(cmd);r=subprocess.run(cmd,env=env,capture_output=True,text=True,timeout=240);assert not r.returncode,r.stderr+r.stdout
    for source,target in [('checker_memory_full_leaf.c','leaf'),('checker_memory_full_init.c','init')]:run([str(b.ZIG),'cc','-std=c11','-O2','-Wall','-Wextra','-Werror',*(['-DCHECKER_IMPOSTOR=1'] if args.impostor else []),'-target','aarch64-linux-musl','-static','-s',str(out/source),'-o',str(out/target)])
    (out/'module').write_bytes(b'INERT_TEST_MAPPING_ONLY\n'+b'\0'*(4096-24))
    header=(out/'native_runtime_tables.hpp').read_text(encoding='utf-8') if 'native_runtime_tables.hpp' in frozen else ''
    def table_digest(symbol,legacy):
        if not header:return legacy
        found=re.findall(r'\b'+symbol+r'\s*(?:\[\])?\s*=\s*"([0-9a-f]{64})"',header)
        assert len(found)==1,(symbol,found)
        return found[0]
    replacements={
        '9df3e1da3832cc2a6626c281f605d68314202f19a622e3126fb53b408909be94':b.sha(out/'module'),
        '8c086e9c2a340c2a388e92ae3de39d6d0e5ce19ac3d582626112a24fd3b97aef':b.sha(out/'leaf'),
        table_digest('pixel_fix_3fr_sha','757be6feaee279ef6fe2e8c6babcec19b616aabae0a173a2fb78efb983658db8'):b.sha(out/'leaf'),
        table_digest('pixel_inputs_sha','af005e43e8f6d29e87b10e968ecf42e92fd5a886593661dbe98d15b6d5556295'):b.sha(out/'leaf'),
        '9521d4cf0a2ea01b039d71d991d507042c518c86c477ca7d5295912c19314b28':b.sha(out/'leaf')}
    text=(out/'native_memory_runner.cpp').read_text(encoding='utf-8');counts={}
    for old,new in replacements.items():
        counts[old]={'runner':text.count(old),'header':header.count(old)};assert sum(counts[old].values())>0
        text=text.replace(old,new);header=header.replace(old,new)
    if header:
        (out/'native_runtime_tables_model.hpp').write_text(header,encoding='utf-8')
        text=text.replace('#include "native_runtime_tables.hpp"','#include "native_runtime_tables_model.hpp"')
    (out/'native_memory_runner_model.cpp').write_text(text,encoding='utf-8')
    run([str(b.ZIG),'cc','-std=c11','-O2','-target','aarch64-linux-musl','-c',str(out/'camera_native.c'),'-o',str(out/'core.o')])
    run([str(b.ZIG),'c++','-std=c++17','-O2','-target','aarch64-linux-musl','-static','-s','-I'+str(out),str(out/'checker_memory_full_adapter.cpp'),str(out/'native_memory_runner_model.cpp'),str(out/'native_common.cpp'),str(out/'core.o'),'-o',str(out/'model')])
    entries=[(n,b'',0o40755) for n in ['dev','proc','tmp','mnt','system','bin']]+[(n,(out/n).read_bytes(),0o100755) for n in ['init','leaf','exact','model']]+[('module',(out/'module').read_bytes(),0o100400)]
    (out/'initramfs.cpio').write_bytes(b.cpio(entries));cmd=[str(b.DEPS/'qemu/qemu-system-aarch64.exe'),'-machine','virt','-cpu','cortex-a53','-m','512','-smp','2','-nic','none','-display','none','-monitor','none','-serial','stdio','-no-reboot','-kernel',str(b.DEPS/'vmlinuz-virt'),'-initrd',str(out/'initramfs.cpio'),'-append','console=ttyAMA0 rdinit=/init panic=-1 quiet']
    r=subprocess.run(cmd,capture_output=True,text=True,timeout=300);(out/'console.txt').write_text(r.stdout+r.stderr,encoding='utf-8');passed=r.returncode==0 and 'MEMORY_FULL_ALL_PASSED' in r.stdout and 'MEMORY_FULL_FAIL' not in r.stdout
    report=dict(passed=passed,exact_artifact_sha256=expected,model_sha256=b.sha(out/'model'),source_sha256={n:b.sha(out/n) for n in names+frozen},fixed_hash_replacements=replacements,replacement_counts=counts,leaf_entries=['cn_prepare_main','cn_counter_main','cn_render_main','cn_publish_main','cn_heif_restart_main'],real_common=True,real_runner_control=True,full_production_binary=False,commands=commands,qemu_command=cmd,real_camera=False,network=False,usb=False,host_shared_directory=False)
    report['checker_sha256']=b.sha(Path(__file__))
    report['model_source_sha256']={'native_memory_runner_model.cpp':b.sha(out/'native_memory_runner_model.cpp')}
    if header:report['model_source_sha256']['native_runtime_tables_model.hpp']=b.sha(out/'native_runtime_tables_model.hpp')
    (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');print(r.stdout[-20000:]);print(out);assert passed
if __name__=='__main__':main()
