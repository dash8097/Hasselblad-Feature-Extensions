"""独立裸Bionic构建检查；原厂profile生成和编译，不装入/运行相机GUI。"""
from pathlib import Path
import hashlib,json,os,re,subprocess,sys
sys.dont_write_bytecode=True
import face_build_arm as base
from firmware_image import system_elf,FirmwareElf
import generate_factory_proxy,face_generate_factory,face_generate_live
HERE=base.HERE;ROOT=base.ROOT
GUI_SHA='16391452abdc69de9e0807e065c0f4ab3f1ccb5fc288f6fc4e6f5cb3bdca12e0'
def sha(data):return hashlib.sha256(data).hexdigest()
def main():
 assert Path.cwd().resolve()==ROOT
 names=['face_live_attachment.cpp','face_live_route.cpp','face_live_profile.cpp','face_draw_pool.cpp','face_view.cpp','face_model.cpp','face_session.cpp','face_io.c','face_factory.cpp','protocol_status.cpp','stock_factory.cpp','abi_view_api.cpp','signal_bridge.cpp','binding_lease.cpp','screen_key_profile.cpp','menu_discovery.cpp','factory_proxy.cpp']
 files={};queue=[HERE/n for n in names]
 while queue:
  path=queue.pop().resolve();assert path.is_relative_to(HERE.resolve());name=path.name;data=path.read_bytes()
  if name in files:assert files[name]==data;continue
  files[name]=data
  for included in re.findall(rb'^\s*#include\s*"([^"\r\n]+)"',data,re.M):queue.append(path.parent/included.decode())
 for name in ('build_face_live_arm_check.py','face_generate_live.py','face_generate_factory.py','generate_factory_proxy.py'):files[name]=(HERE/name).read_bytes()
 binary=system_elf('/bin/camera-gui',GUI_SHA);evidence={}
 for name,module in [('proxy_generated.hpp',generate_factory_proxy),('factory_generated.hpp',face_generate_factory),('live_generated.hpp',face_generate_live)]:files[name],evidence[name]=module.generate(binary)
 files['profile_probe.cpp']=b'''#include <cstdint>
struct UiCode {uintptr_t address;unsigned char bytes[16];};
#include "proxy_generated.hpp"
#include "factory_generated.hpp"
#include "live_generated.hpp"
x2dui::FaceLiveProfile fixed_face_profile_probe(uintptr_t base){return face_live_profile(base);}
'''
 names.append('profile_probe.cpp')
 deps={'libc.so':base.LIBC,'libm.so':ROOT/'x2d/outputs/4.2.0/subject-tracking/link-inputs/libm.so'}
 identity=dict(sources={n:sha(v) for n,v in files.items()},gui=GUI_SHA,dependencies={n:base.sha(p) for n,p in deps.items()},compiler=base.sha(base.ZIG))
 revision=sha(json.dumps(identity,sort_keys=True).encode())[:20]
 out=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/native-ui/face-live-arm'/revision;source=out/'private-build/source';source.mkdir(parents=True,exist_ok=False)
 for n,data in files.items():(source/n).write_bytes(data)
 for n,p in deps.items():(source/n).write_bytes(p.read_bytes())
 env=dict(os.environ,TEMP=str(source.parent),TMP=str(source.parent),ZIG_LOCAL_CACHE_DIR=str(source.parent/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(source.parent/'zig-global'))
 commands=[];objects=[]
 def run(cmd):
  commands.append(list(map(str,cmd)));r=subprocess.run(commands[-1],env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=120);(out/('command-%d.log'%len(commands))).write_text(r.stdout+r.stderr,encoding='utf8');assert not r.returncode,r.stdout+r.stderr
 for name in names:
  obj=source.parent/(name+'.o');objects.append(obj);cpp=name.endswith('.cpp')
  run([base.ZIG,'c++' if cpp else 'cc','-target','aarch64-linux-gnu','-O2','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-const-variable','-Wno-misleading-indentation','-fPIC','-fvisibility=hidden','-fno-builtin','-fno-stack-protector','-mno-outline-atomics','-D__errno_location=__errno',*(['-std=c++17','-fno-exceptions','-fno-rtti'] if cpp else ['-std=c11']),'-I',source,'-c',source/name,'-o',obj])
 artifact=out/'face-live-link-probe.so'
 run([base.ZIG,'ld.lld','-m','aarch64elf','-shared','--no-undefined','--allow-shlib-undefined','-z','noexecstack','--strip-all',*objects,*[source/n for n in deps],'-o',artifact])
 elf=FirmwareElf(artifact.read_bytes()).elf
 imports={s.name for s in elf.get_section_by_name('.dynsym').iter_symbols() if s.name and s['st_shndx']=='SHN_UNDEF'}
 supported={s.name for name in deps for s in FirmwareElf((source/name).read_bytes()).elf.get_section_by_name('.dynsym').iter_symbols() if s.name and s['st_shndx']!='SHN_UNDEF'}
 assert imports<=supported,imports-supported
 needed=[t.needed for t in elf.get_section_by_name('.dynamic').iter_tags() if t.entry.d_tag=='DT_NEEDED'];assert set(needed)<=set(deps),needed
 changed=[name for name,data in files.items() if (HERE/name).is_file() and (HERE/name).read_bytes()!=data]
 report=dict(identity=identity,evidence=evidence,linked=True,imports=sorted(imports),needed=needed,artifact_sha256=base.sha(artifact),bytes=artifact.stat().st_size,source_matches_live_at_finish=not changed,changed_source_during_build=changed,commands=commands,independent_build_check=True,arm_executed=False,original_gui_attached=False,hardware_operations=0)
 (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8');print(out);print('BARE_BIONIC_FACE_LIVE_LINK_PASS',sorted(imports))
if __name__=='__main__':main()
