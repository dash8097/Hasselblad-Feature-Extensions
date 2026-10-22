"""作者离线运行验证：精确 ELF，隔离 ARM64 Linux，不连接设备或主机目录。"""
import argparse,json,os,shutil,subprocess
from pathlib import Path
import checker_linux_env as base


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('revision')
    args=parser.parse_args()
    assert len(args.revision)==20 and all(c in '0123456789abcdef' for c in args.revision)
    assert Path.cwd().resolve()==base.ROOT
    root=base.ROOT/'x2d/outputs/4.2.0/installer-hardening-r7'
    revision=root/'native-flow/revisions'/args.revision
    proof=json.loads((revision/'build-proof.json').read_text(encoding='utf-8'))
    artifact=revision/'artifacts/x2d-native-flow'
    assert base.sha(artifact)==proof['artifact']['sha256']
    source=base.HERE/'test_data_cleanup_init.c'
    ident=base.sha(source)[:12]
    out=root/'native-data-cleanup/tests'/f'{args.revision}-{ident}'
    assert not (out/'report.json').exists(),'不可覆盖旧证据'
    out.mkdir(parents=True,exist_ok=True)
    shutil.copyfile(source,out/source.name)
    env=dict(os.environ,TEMP=str(out),TMP=str(out),ZIG_LOCAL_CACHE_DIR=str(out/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(out/'zig-global'))
    result=subprocess.run([str(base.ZIG),'cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-target','aarch64-linux-musl','-static','-s',str(out/source.name),'-o',str(out/'init')],env=env,capture_output=True,text=True,timeout=180)
    assert result.returncode==0,result.stderr+result.stdout
    entries=[(p,b'',0o40755) for p in ['dev','proc','tmp','data','system','mnt']]
    entries += [('init',(out/'init').read_bytes(),0o100755),('flow',artifact.read_bytes(),0o100755)]
    legacy= root/'native-flow/revisions/127dcff48c8b649dfcd8/artifacts/x2d-native-flow'
    assert base.sha(legacy)=='59f5890266bf3d9a4d4a6c99e5cda63a8b77f9c1ba2898602ea459c018332da1'
    entries += [('fixtures',b'',0o40755),('fixtures/legacy-helper',legacy.read_bytes(),0o100755)]
    recent_helpers=[]
    for i,rev in enumerate(('7095c9d452ee221a9456','289efb57d81a6d247db4','5aaa3646850e3fbdaf58','9a2a48d1971d09cb861d')):
        helper=root/'native-flow/revisions'/rev/'artifacts/x2d-native-flow'
        hp=json.loads((helper.parent.parent/'build-proof.json').read_text(encoding='utf8'))
        assert base.sha(helper)==hp['artifact']['sha256'] and hp['build_identity']['profile']=='setup'
        recent_helpers.append(base.sha(helper))
        entries.append(('fixtures/recent-helper-'+str(i),helper.read_bytes(),0o100755))
    oldlib=base.ROOT/'x2d/outputs/4.2.0/raw-stack/continuous-candidate/capture/revisions/90b7e3f1779c6a088d6d/artifacts/libx2d_raw_stack.so'
    assert base.sha(oldlib)=='463f6057ca9fd08e9e5b1215720a6c85bcf2968f28b274da682c72526a1c2d37'
    entries += [('fixtures/legacy-library',oldlib.read_bytes(),0o100644)]
    (out/'initramfs.cpio').write_bytes(base.cpio(entries))
    command=[str(base.DEPS/'qemu/qemu-system-aarch64.exe'),'-machine','virt','-cpu','cortex-a53','-m','512','-smp','2','-nic','none','-display','none','-monitor','none','-serial','stdio','-no-reboot','-kernel',str(base.DEPS/'vmlinuz-virt'),'-initrd',str(out/'initramfs.cpio'),'-append','console=ttyAMA0 rdinit=/init panic=-1 quiet']
    result=subprocess.run(command,capture_output=True,text=True,timeout=180)
    (out/'console.txt').write_text(result.stdout+result.stderr,encoding='utf-8')
    passed=result.returncode==0 and 'DATA_CLEANUP_ALL_PASSED' in result.stdout and 'DATA_CHECK_FAILED' not in result.stdout
    report=dict(passed=passed,independent_checker=False,artifact_sha256=base.sha(artifact),source_sha256=proof['source_sha256'],test_sha256=base.sha(source),kernel_sha256=base.sha(base.DEPS/'vmlinuz-virt'),command=command,real_camera=False,network=False,usb=False,host_shared_directory=False,returncode=result.returncode)
    report['recent_helper_sha256']=recent_helpers
    (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(result.stdout[-10000:]);print(out)
    assert passed


if __name__=='__main__':
    main()
