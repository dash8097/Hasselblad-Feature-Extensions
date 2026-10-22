"""裸 Bionic 链接验收：核心不引入第二套 C++ runtime，不接入相机进程。"""
from pathlib import Path
import hashlib,json,os,subprocess,sys
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[3]
sys.path.insert(0,str(ROOT/'x2d/tools'))
from firmware_image import FirmwareElf
ZIG=ROOT/'.research-cache/x1d-1.25.0/toolchain/zig-windows-x86_64-0.13.0/zig.exe'
LIBC=ROOT/'x2d/outputs/4.2.0/raw-stack/backend-r1/libc.so'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 assert Path.cwd().resolve()==ROOT
 paths=[HERE/n for n in ('screen_mode_attachment.hpp','screen_mode_attachment.cpp','screen_mode_session.hpp','screen_mode_session.cpp','screen_notice_view.hpp','screen_notice_view.cpp', 'screen_key_profile.hpp', 'screen_key_profile.cpp', 'screen_factory_profile.hpp', 'screen_factory_profile.cpp', 'screen_controls_view.hpp', 'screen_controls_view.cpp', 'screen_controls.hpp', 'screen_controls.cpp', 'screen_policy.hpp', 'screen_policy.cpp', 'factory_proxy.hpp', 'factory_proxy.cpp', 'binding_lease.hpp', 'binding_lease.cpp', 'signal_bridge.hpp', 'signal_bridge.cpp', 'stock_factory.hpp', 'stock_factory.cpp', 'abi_view_api.hpp', 'abi_view_api.cpp', 'key_filter.hpp', 'key_mapping.hpp', 'factory_formatter.hpp', 'protocol_text.hpp', 'menu_discovery.hpp', 'effect_view_core.hpp', 'local_heap.hpp', 'screen_build_mode_arm.py')]
 identity={p.name:sha(p) for p in paths};identity['libc']=sha(LIBC);identity['compiler']=sha(ZIG)
 rev=hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()[:20]
 out=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/native-ui/screen-mode-attachment-arm'/rev;src=out/'private-build/source';src.mkdir(parents=True,exist_ok=False)
 for p in paths:(src/p.name).write_bytes(p.read_bytes())
 (src/'libc.so').write_bytes(LIBC.read_bytes())
 env=dict(os.environ,TEMP=str(src.parent),TMP=str(src.parent),ZIG_LOCAL_CACHE_DIR=str(src.parent/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(src.parent/'zig-global'))
 commands=[]
 def run(cmd):
  commands.append(list(map(str,cmd)));r=subprocess.run(commands[-1],env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=120);(out/('command-%d.log'%len(commands))).write_text(r.stdout+r.stderr,encoding='utf8');assert not r.returncode,r.stdout+r.stderr
 objs=[]
 for n in ('screen_mode_attachment.cpp','screen_mode_session.cpp','screen_notice_view.cpp', 'screen_key_profile.cpp', 'screen_factory_profile.cpp', 'screen_controls_view.cpp', 'screen_controls.cpp', 'screen_policy.cpp', 'factory_proxy.cpp', 'binding_lease.cpp', 'signal_bridge.cpp', 'stock_factory.cpp', 'abi_view_api.cpp'):
  obj=src.parent/(n+'.o');objs.append(obj)
  run([ZIG,'c++','-target','aarch64-linux-gnu','-std=c++17','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fPIC','-fvisibility=hidden','-fno-builtin','-fno-stack-protector','-mno-outline-atomics','-fno-exceptions','-fno-rtti','-I',src,'-c',src/n,'-o',obj])
 binary=out/'screen-mode-link-probe.so'
 run([ZIG,'ld.lld','-m','aarch64elf','-shared','--no-undefined','--allow-shlib-undefined','-z','noexecstack','--strip-all',*objs,src/'libc.so','-o',binary])
 elf=FirmwareElf(binary.read_bytes()).elf;imports={s.name for s in elf.get_section_by_name('.dynsym').iter_symbols() if s.name and s['st_shndx']=='SHN_UNDEF'}
 supported={s.name for s in FirmwareElf(LIBC.read_bytes()).elf.get_section_by_name('.dynsym').iter_symbols() if s.name and s['st_shndx']!='SHN_UNDEF'}
 assert imports<=supported,imports-supported
 needed=[t.needed for t in elf.get_section_by_name('.dynamic').iter_tags() if t.entry.d_tag=='DT_NEEDED'];assert needed==['libc.so'],needed
 assert all(sha(p)==identity[p.name] for p in paths)
 (out/'report.json').write_text(json.dumps(dict(identity=identity,artifact_sha256=sha(binary),bytes=binary.stat().st_size,imports=sorted(imports),needed=needed,commands=commands,linked=True,arm_executed=False,qt_attached=False,hardware_operations=0,author_tests=True),ensure_ascii=False,indent=2),encoding='utf8')
 print(out);print('BARE_BIONIC_LINK_PASS',sorted(imports))
if __name__=='__main__':main()
