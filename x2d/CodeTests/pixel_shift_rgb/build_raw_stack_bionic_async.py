"""为已验证候选构建 Bionic 常驻工人小数据检查器，不访问设备。"""
import hashlib,json,os,subprocess,sys
from pathlib import Path
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[2]
OUT=ROOT/'x2d/outputs/4.2.0/raw-stack/backend-r1'
sys.path.insert(0,str(ROOT/'x2d/tools'))
from firmware_image import FirmwareElf
zig=ROOT/'.research-cache/x1d-1.25.0/toolchain/zig-windows-x86_64-0.13.0/zig.exe'
env=dict(os.environ,ZIG_LOCAL_CACHE_DIR=str(OUT/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(OUT/'zig-global'))
def run(args):subprocess.run([str(a) for a in args],env=env,check=True,timeout=120)
obj=OUT/'check-stack-bionic.o';target=OUT/'check-stack-bionic'
run([zig,'cc','-target','aarch64-linux-gnu','-O2','-Wall','-Wextra','-Werror','-fPIE','-fno-stack-protector','-mno-outline-atomics','-c',HERE/'check_raw_stack_bionic_async.c','-o',obj])
run([zig,'ld.lld','-m','aarch64elf','-pie','--no-undefined','--allow-shlib-undefined','-z','noexecstack','-e','_start','--strip-all','--dynamic-linker=/system/bin/linker64',obj,OUT/'libx2d_raw_stack.so',OUT/'libc.so','-o',target])
b=FirmwareElf(target.read_bytes());assert [s.data() for s in b.elf.iter_segments() if s['p_type']=='PT_INTERP']==[b'/system/bin/linker64\0']
imports={s.name for s in b.elf.get_section_by_name('.dynsym').iter_symbols() if s.name and s['st_shndx']=='SHN_UNDEF'}
exports=set()
for name in ('libx2d_raw_stack.so','libc.so'):
 e=FirmwareElf((OUT/name).read_bytes());exports.update(s.name for s in e.elf.get_section_by_name('.dynsym').iter_symbols() if s['st_shndx']!='SHN_UNDEF')
assert imports<=exports,imports-exports
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
(OUT/'bionic-async-build.json').write_text(json.dumps(dict(offline=True,probe_sha256=sha(target),module_sha256=sha(OUT/'libx2d_raw_stack.so'),source_sha256=sha(HERE/'check_raw_stack_bionic_async.c'),imports=sorted(imports),device_verified=False),indent=2)+'\n')
print('RAW_STACK_BIONIC_ASYNC_CHECK_BUILT_NO_CAPTURE')
