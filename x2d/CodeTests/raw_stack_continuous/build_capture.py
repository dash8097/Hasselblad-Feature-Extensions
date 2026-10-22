"""构建连续曝光候选：原厂4.2.0绑定、纯16位NEON极值和冻结H3依赖。仅离线。"""
from pathlib import Path
import hashlib,json,os,re,subprocess,sys
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[2]
sys.path.insert(0,str(ROOT/'x2d/CodeTests/installer_hardening/camera_native'))
import build_pilot
BASE=ROOT/'x2d/outputs/4.2.0/installer-hardening-r3/build/iso-frozen'
OUT=ROOT/'x2d/outputs/4.2.0/raw-stack/continuous-candidate/capture'
BINDING=ROOT/'x2d/outputs/4.2.0/pixel-shift-rgb/storage_private_bindings.inc'
DEPS={
    'libc.so':ROOT/'x2d/outputs/4.2.0/raw-stack/backend-r1/libc.so',
    'libdl.so':ROOT/'x2d/outputs/4.2.0/pixel-shift-rgb/memory-session-r1/libdl.so',
}
UNITS=['native_stack_capture.c','raw_stack_core.c','raw_stack_parallel.c','raw_stack_async.c','raw_stack_file.c',
       'capture_memory_native.c','factory_private_frame.c','capture_header_export.c','capture_header_native.c',
       'capture_metadata_native.c','factory_capture_identity.c','factory_private_frame_abi.S']
def sha(data):return hashlib.sha256(data).hexdigest()
def main():
    assert Path.cwd().resolve()==ROOT
    baseline=json.loads((BASE/'source-snapshot.json').read_text(encoding='utf-8'))
    files={};paths={}
    def include(name):
        assert '/' not in name and '\\' not in name
        if name in files:return
        path=HERE/name if name in ('native_stack_capture.c','native_stack_abort.inc','raw_stack_dma.h') else ROOT/'x2d/CodeTests/pixel_shift_rgb'/name if name in ('raw_stack_core.c','raw_stack_core.h','raw_stack_async.c') else BINDING if name==BINDING.name else BASE/'source'/name
        data=path.read_bytes()
        if path.parent==BASE/'source':assert sha(data)==baseline[name],name
        files[name]=data;paths[name]=path
        for dep in re.findall(r'^\s*#include\s+"([^"]+)"',data.decode('utf-8-sig'),re.M):include(dep)
    for unit in UNITS:include(unit)
    for name,path in DEPS.items():files[name]=path.read_bytes();paths[name]=path
    identity=dict(sources={n:sha(b) for n,b in files.items()},builder=sha(Path(__file__).read_bytes()),zig=build_pilot.sha(build_pilot.ZIG),firmware='X2D 4.2.0')
    revision=sha(json.dumps(identity,sort_keys=True).encode())[:20]
    out=OUT/'revisions'/revision;private=out/'private-build';source=private/'source';binary=out/'artifacts/libx2d_raw_stack.so';proof=out/'build-proof.json'
    if proof.exists():
        report=json.loads(proof.read_text(encoding='utf-8'));assert report['identity']==identity
        for name,digest in identity['sources'].items():assert build_pilot.sha(source/name)==digest
        assert build_pilot.sha(binary)==report['artifact']['sha256']
        print(json.dumps(dict(directory=str(out),artifact=report['artifact'],reused=True)));return
    source.mkdir(parents=True,exist_ok=True);binary.parent.mkdir(exist_ok=True)
    for name,data in files.items():(source/name).write_bytes(data)
    env=dict(os.environ,TEMP=str(private),TMP=str(private),ZIG_LOCAL_CACHE_DIR=str(private/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(private/'zig-global'))
    commands=[]
    def run(args):
        command=[str(v) for v in args];commands.append(command)
        result=subprocess.run(command,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=180)
        assert result.returncode==0,result.stdout+result.stderr
    objects=[]
    for name in UNITS:
        obj=private/(name+'.o');objects.append(obj)
        run([build_pilot.ZIG,'cc','-O2','-Wall','-Wextra','-Werror','-pthread','-DRS_ASYNC_DIRECT=1','-target','aarch64-linux-gnu','-fPIC','-fno-stack-protector','-mno-outline-atomics','-D__errno_location=__errno','-I',source,'-c',source/name,'-o',obj])
    run([build_pilot.ZIG,'ld.lld','-m','aarch64elf','-shared','--no-undefined','--allow-shlib-undefined','-z','noexecstack','--strip-all','-soname',binary.name,*objects,*[source/n for n in DEPS],'-o',binary])
    sys.path.insert(0,str(ROOT/'x2d/tools'))
    from firmware_image import FirmwareElf
    elf=FirmwareElf(binary.read_bytes()).elf
    assert elf['e_machine']=='EM_AARCH64' and elf['e_type']=='ET_DYN'
    imports={s.name for s in elf.get_section_by_name('.dynsym').iter_symbols() if s.name and s['st_shndx']=='SHN_UNDEF'}
    exports=set()
    for name in DEPS:
        e=FirmwareElf((source/name).read_bytes()).elf
        exports.update(s.name for s in e.get_section_by_name('.dynsym').iter_symbols() if s['st_shndx']!='SHN_UNDEF')
    assert imports<=exports,imports-exports
    needed=[t.needed for t in elf.get_section_by_name('.dynamic').iter_tags() if t.entry.d_tag=='DT_NEEDED']
    assert sorted(needed)==sorted(DEPS),needed
    assert not any(p['p_type']=='PT_INTERP' for p in elf.iter_segments())
    for name,path in paths.items():assert build_pilot.sha(path)==identity['sources'][name],'输入在构建期间变化：'+name
    artifact=dict(path=str(binary.relative_to(ROOT)),sha256=build_pilot.sha(binary),bytes=binary.stat().st_size)
    report=dict(identity=identity,artifact=artifact,imports=sorted(imports),needed=needed,offline=True,camera_requests=0,hardware_tested=False,release_ready=False,commands=commands)
    proof.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    (OUT/'latest-build.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(directory=str(out),artifact=artifact),ensure_ascii=False))
if __name__=='__main__':main()
