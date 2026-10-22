"""冻结4.2后端自有副本的只读确认协议作者模型与裸Bionic链接；不加载到设备。"""
from pathlib import Path
import hashlib,json,os,subprocess,sys
sys.dont_write_bytecode=True
import build_processing_tests as b
from face_build_arm import ZIG,LIBC
sys.path.insert(0,str(b.ROOT/'x2d/tools'))
from firmware_image import FirmwareElf
def main():
 assert Path.cwd().resolve()==b.ROOT
 src=b.HERE/'face_backend_v2';names=['face_lock_backend.c','face_lock_policy.h','eye_af_bridge.h','eye_age_limit.h','eye_test.c','origin.json']
 paths=[src/n for n in names]+[Path(__file__)]
 libdl=b.ROOT/'x2d/outputs/4.2.0/afc-observer-r1/libdl.so'
 ident={p.name:b.sha(p) for p in paths};ident.update(zig=b.sha(ZIG),hostcc=b.sha(b.CXX),libc=b.sha(LIBC),libdl=b.sha(libdl))
 rev=hashlib.sha256(json.dumps(ident,sort_keys=True).encode()).hexdigest()[:20];out=b.OUT/'native-ui/face-backend-v2'/rev;frozen=out/'private-source';frozen.mkdir(parents=True,exist_ok=False)
 for p in paths:(frozen/p.name).write_bytes(p.read_bytes())
 env=dict(os.environ,TEMP=str(out),TMP=str(out),PATH=str(b.CXX.parent)+os.pathsep+os.environ['PATH'],ZIG_GLOBAL_CACHE_DIR=str(out/'zig-global'),ZIG_LOCAL_CACHE_DIR=str(out/'zig-local'))
 commands=[]
 def run(cmd):
  commands.append(list(map(str,cmd)));r=subprocess.run(commands[-1],env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=120);(out/('command-%d.log'%len(commands))).write_text(r.stdout+r.stderr,encoding='utf8');assert not r.returncode,r.stdout+r.stderr;return r.stdout
 run([b.CXX.parent/'gcc.exe','-std=c11','-O2','-Wall','-Wextra','-Werror','-Wno-unused-variable',frozen/'eye_test.c','-o',out/'test.exe']);stdout=run([out/'test.exe'])
 obj=out/'backend.o';binary=out/'libx2d_face_lock_v2.so'
 run([ZIG,'cc','-target','aarch64-linux-gnu','-std=c11','-O2','-Wall','-Wextra','-Werror','-Wno-unused-function','-DFACE_EYE_BACKEND','-fPIC','-fno-stack-protector','-mno-outline-atomics','-c',frozen/'face_lock_backend.c','-o',obj])
 run([ZIG,'ld.lld','-m','aarch64elf','-shared','--no-undefined','--allow-shlib-undefined','-z','noexecstack','--strip-all','-soname','libx2d_face_lock.so',obj,LIBC,libdl,'-o',binary])
 elf=FirmwareElf(binary.read_bytes()).elf;imports=sorted(s.name for s in elf.get_section_by_name('.dynsym').iter_symbols() if s.name and s['st_shndx']=='SHN_UNDEF')
 assert all(b.sha(p)==ident[p.name] for p in paths)
 (out/'report.json').write_text(json.dumps(dict(identity=ident,artifact_sha256=b.sha(binary),bytes=binary.stat().st_size,imports=imports,commands=commands,stdout=stdout,author_tests=True,hardware_operations=0,arm_executed=False,integrated=False,confirmation='CLOCK_MONOTONIC毫秒下的新鲜状态观测；不是事务nonce或曝光/镜头操作确认'),ensure_ascii=False,indent=2),encoding='utf8')
 print(out);print(stdout)
if __name__=='__main__':main()
