"""动画纯绘图层ARM64裸链接/布局检查；不等价原厂QPainter地址绑定或实际paint挂接。"""
from pathlib import Path
import hashlib,json,os,subprocess,sys
sys.dont_write_bytecode=True
import build_pixel_arm as b
def main():
 assert Path.cwd().resolve()==b.ROOT
 paths=[b.HERE/n for n in ('animation_view.hpp','animation_view.cpp','shutter_animation_core.hpp','build_animation_view_arm.py')]
 identity={p.name:b.sha(p) for p in paths};identity['compiler']=b.sha(b.ZIG)
 rev=hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()[:20];out=b.ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/native-ui/animation-view-arm'/rev;src=out/'private-build/source';src.mkdir(parents=True,exist_ok=False)
 for p in paths:(src/p.name).write_bytes(p.read_bytes())
 (src/'layout.cpp').write_text('#include "animation_view.hpp"\nusing namespace x2dui;\nstatic_assert(sizeof(AnimationRect)==32&&sizeof(AnimationPoint)==16&&sizeof(AnimationTextMetrics)==24&&sizeof(AnimationPathPart)==40,"POD ABI");\n',encoding='utf8')
 env=dict(os.environ,TEMP=str(src.parent),TMP=str(src.parent),ZIG_LOCAL_CACHE_DIR=str(src.parent/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(src.parent/'zig-global'));commands=[]
 def run(cmd):
  commands.append(list(map(str,cmd)));r=subprocess.run(commands[-1],env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=120);(out/('command-%d.log'%len(commands))).write_text(r.stdout+r.stderr,encoding='utf8');assert not r.returncode,r.stdout+r.stderr
 objs=[]
 for name in ('animation_view.cpp','layout.cpp'):
  obj=src.parent/(name+'.o');objs.append(obj);run([b.ZIG,'c++','-target','aarch64-linux-gnu','-std=c++17','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fPIC','-fvisibility=hidden','-fno-builtin','-fno-stack-protector','-mno-outline-atomics','-fno-exceptions','-fno-rtti','-I',src,'-c',src/name,'-o',obj])
 binary=out/'animation-view-link-probe.so';run([b.ZIG,'ld.lld','-m','aarch64elf','-shared','--no-undefined','-z','noexecstack','--strip-all',*objs,'-o',binary])
 elf=b.FirmwareElf(binary.read_bytes()).elf;imports=[s.name for s in elf.get_section_by_name('.dynsym').iter_symbols() if s.name and s['st_shndx']=='SHN_UNDEF'];assert not imports,imports
 assert all(b.sha(p)==identity[p.name] for p in paths)
 (out/'report.json').write_text(json.dumps(dict(identity=identity,artifact_sha256=b.sha(binary),bytes=binary.stat().st_size,imports=imports,commands=commands,linked=True,arm_executed=False,stock_painter_profile_bound=False,qt_attached=False,hardware_operations=0,author_tests=True),ensure_ascii=False,indent=2),encoding='utf8');print(out)
if __name__=='__main__':main()
