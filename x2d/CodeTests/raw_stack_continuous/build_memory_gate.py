"""冻结既有 4.2.0 六帧源码，仅替换请求握手；离线候选，非正式发行。"""
from pathlib import Path
import hashlib,json,os,re,subprocess,sys
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[2]
NATIVE=ROOT/'x2d/CodeTests/installer_hardening/camera_native'
sys.path.insert(0,str(NATIVE))
import build_pilot
from elf_metadata_hardening import normalize_elf
BASE=ROOT/'x2d/outputs/4.2.0/pixel-shift-rgb/compat-source-archive-r1'
OUT=ROOT/'x2d/outputs/4.2.0/raw-stack/continuous-candidate/memory-gate'
def sha(b):return hashlib.sha256(b).hexdigest()
def main():
 assert Path.cwd().resolve()==ROOT
 assert json.loads((BASE/'build.json').read_text())['capture']['sha256']=='93769d90ddb345768aa2a4f876a372b65c74849a65c4a13581e12bfcf5ac3cab'
 files={p.name:p.read_bytes() for p in (BASE/'capture/src').iterdir() if p.is_file()}
 text=files['private_capture_preload.c'].decode().replace('\r\n','\n')
 start=text.index('  char requested[256];')
 end=text.index('   else {\n    log_line("PRIVATE_CAPTURE_OWN_REQUEST_ARMED',start)
 old=text[start:end]
 assert 'read(fd,text,sizeof text)' in old and 'x2d_capture_group_request_issued' in old
 text=text[:start]+'''  int request=x2d_memory_request_gate(X2D_PRIVATE_CAPTURE_JOB);
  if(request){
   if(request<0||!x2d_capture_group_request_issued(&group,1)||!owned_marker("owner.ready"))
    failure("PRIVATE_CAPTURE_OWN_REQUEST_MARKER_REJECTED\\n");
'''+text[end:]
 files['private_capture_preload.c']=('#include "memory_request_gate.h"\n'+text).encode()
 files['memory_request_gate.h']=(HERE/'memory_request_gate.h').read_bytes()
 files['compat_memory_paths.h']=(ROOT/'x2d/CodeTests/pixel_shift_rgb/compat_memory_paths.h').read_bytes()
 deps={'libc.so':'x2d/outputs/4.2.0/raw-stack/backend-r1/libc.so','libdl.so':'x2d/outputs/4.2.0/pixel-shift-rgb/memory-session-r1/libdl.so','storage_private_bindings.inc':'x2d/outputs/4.2.0/pixel-shift-rgb/storage_private_bindings.inc'}
 for name,path in deps.items():files[name]=(ROOT/path).read_bytes()
 identity=dict(builder=sha(Path(__file__).read_bytes()),sources={n:sha(b) for n,b in files.items()},baseline=sha((BASE/'build.json').read_bytes()))
 revision=sha(json.dumps(identity,sort_keys=True).encode())[:20]
 out=OUT/revision;src=out/'source';src.mkdir(parents=True,exist_ok=True)
 for n,b in files.items():(src/n).write_bytes(b)
 env=dict(os.environ,TEMP=str(out),TMP=str(out),ZIG_LOCAL_CACHE_DIR=str(out/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(out/'zig-global'))
 units=[p.stem+'.c' for p in (BASE/'capture').glob('*.o')]
 units[units.index('factory_private_frame_abi.c')]='factory_private_frame_abi.S'
 objects=[];commands=[]
 def run(args):
  cmd=[str(v) for v in args];commands.append(cmd);r=subprocess.run(cmd,env=env,capture_output=True,text=True,timeout=180);assert not r.returncode,r.stdout+r.stderr
 for name in units:
  obj=out/(name+'.o');objects.append(obj)
  extra=['-DX2D_MEMORY_CAPTURE_SESSION=1','-DX2D_PRIVATE_CAPTURE_FRAMES=6','-DX2D_PRIVATE_CAPTURE_JOB="/mnt/media_rw/ssd/multishot/native-album-ui-memory-r1"'] if name=='private_capture_preload.c' else []
  run([build_pilot.ZIG,'cc','-O2','-Wall','-Wextra','-Werror','-pthread','-target','aarch64-linux-gnu','-fPIC','-fno-stack-protector','-mno-outline-atomics','-D__errno_location=__errno','-DX2D_CAPTURE_BIONIC_PROFILE=1','-I',src,*extra,'-c',src/name,'-o',obj])
 target=out/'libx2d_private_memory_capture.so'
 run([build_pilot.ZIG,'cc','-O2','-pthread','-target','aarch64-linux-gnu','-shared','-fPIC','-nostdlib','-Wl,--no-undefined','-Wl,--allow-shlib-undefined','-Wl,-z,noexecstack','-Wl,--strip-all','-Wl,-soname,libx2d_private_memory_capture.so',*objects,src/'libc.so',src/'libdl.so','-o',target])
 data,_=normalize_elf(target.read_bytes());target.write_bytes(data)
 sys.path.insert(0,str(ROOT/'x2d/tools'))
 from firmware_image import FirmwareElf
 elf=FirmwareElf(data).elf
 imports={s.name for s in elf.get_section_by_name('.dynsym').iter_symbols() if s.name and s['st_shndx']=='SHN_UNDEF'}
 exports=set()
 for name in ('libc.so','libdl.so'):
  e=FirmwareElf(files[name]).elf
  exports.update(s.name for s in e.get_section_by_name('.dynsym').iter_symbols() if s['st_shndx']!='SHN_UNDEF')
 assert imports<=exports,imports-exports
 needed=[t.needed for t in elf.get_section_by_name('.dynamic').iter_tags() if t.entry.d_tag=='DT_NEEDED']
 assert sorted(needed)==['libc.so','libdl.so'] and elf['e_machine']=='EM_AARCH64'
 assert not any(s['p_type']=='PT_LOAD' and s['p_flags']&3==3 for s in elf.iter_segments())
 report=dict(identity=identity,artifact=dict(path=str(target.relative_to(ROOT)),sha256=sha(data)),imports=sorted(imports),needed=needed,commands=commands,installed=False,hardware_tested=False)
 (out/'build-proof.json').write_text(json.dumps(report,indent=2)+'\n');OUT.mkdir(exist_ok=True);(OUT/'latest-build.json').write_text(json.dumps(report,indent=2)+'\n');print(target)
if __name__=='__main__':main()
