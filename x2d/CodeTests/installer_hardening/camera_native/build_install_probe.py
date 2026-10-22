"""离线构建固定 Bionic 加载探针；不运行设备，不提取新固件内容。"""
from pathlib import Path
import hashlib,json,os,shutil,subprocess,sys
import build_pilot
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
OUT=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/install-probe'
DEPS={
    'libc.so':ROOT/'x2d/outputs/4.2.0/raw-stack/backend-r1/libc.so',
    'libdl.so':ROOT/'x2d/outputs/4.2.0/pixel-shift-rgb/memory-session-r1/libdl.so',
}
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    assert Path.cwd().resolve()==ROOT
    inputs={'source':sha(HERE/'native_install_probe.c'),'builder':sha(Path(__file__)),
            'zig':sha(build_pilot.ZIG),'dependencies':{n:sha(p) for n,p in DEPS.items()}}
    revision=hashlib.sha256(json.dumps(inputs,sort_keys=True).encode()).hexdigest()[:20]
    out=OUT/'revisions'/revision;private=out/'private-build';private.mkdir(parents=True,exist_ok=True)
    source=private/'native_install_probe.c';shutil.copyfile(HERE/source.name,source)
    for name,path in DEPS.items():shutil.copyfile(path,private/name)
    env=dict(os.environ,TEMP=str(private),TMP=str(private),ZIG_LOCAL_CACHE_DIR=str(private/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(private/'zig-global'))
    def run(args):
        r=subprocess.run([str(a) for a in args],env=env,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=180)
        if r.returncode:raise RuntimeError(r.stdout+r.stderr)
    obj=private/'probe.o';artifacts=out/'artifacts';artifacts.mkdir(exist_ok=True);binary=artifacts/'install-probe'
    run([build_pilot.ZIG,'cc','-target','aarch64-linux-gnu','-Oz','-Wall','-Wextra','-Werror','-fPIE','-fno-builtin','-fno-stack-protector','-mno-outline-atomics','-D__errno_location=__errno','-c',source,'-o',obj])
    run([build_pilot.ZIG,'ld.lld','-m','aarch64elf','-pie','--no-undefined','--allow-shlib-undefined','-z','noexecstack','-e','_start','--strip-all','--dynamic-linker=/system/bin/linker64',obj,*[private/n for n in DEPS],'-o',binary])
    sys.path.insert(0,str(ROOT/'x2d/tools'))
    from firmware_image import FirmwareElf
    elf=FirmwareElf(binary.read_bytes()).elf
    assert [p.data() for p in elf.iter_segments() if p['p_type']=='PT_INTERP']==[b'/system/bin/linker64\0']
    imports={s.name for s in elf.get_section_by_name('.dynsym').iter_symbols() if s.name and s['st_shndx']=='SHN_UNDEF'}
    exports=set()
    for name in DEPS:
        e=FirmwareElf((private/name).read_bytes()).elf
        exports.update(s.name for s in e.get_section_by_name('.dynsym').iter_symbols() if s['st_shndx']!='SHN_UNDEF')
    assert imports<=exports,imports-exports
    assert sha(source)==inputs['source'] and all(sha(p)==inputs['dependencies'][n] for n,p in DEPS.items())
    report={'firmware':'X2D 4.2.0','inputs':inputs,'dependency_paths':{n:str(p.relative_to(ROOT)) for n,p in DEPS.items()},
            'artifact':{'path':str(binary.relative_to(ROOT)),'sha256':sha(binary),'bytes':binary.stat().st_size},
            'imports':sorted(imports),'offline':True,'device_executed':False,'qualified':False}
    (out/'build-proof.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    OUT.mkdir(exist_ok=True);(OUT/'latest-build.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(report['artifact']))
if __name__=='__main__':main()
