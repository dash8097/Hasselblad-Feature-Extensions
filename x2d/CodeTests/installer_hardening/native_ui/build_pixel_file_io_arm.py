"""固定文件通信的原厂4.2.0 libc链接检查；不是原厂GUI执行验收。"""
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
 paths=[HERE/n for n in ('pixel_file_io.h','pixel_file_io.c','build_pixel_file_io_arm.py')]
 identity={p.name:sha(p) for p in paths};identity['compiler']=sha(ZIG);identity['libc']=sha(LIBC)
 out=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/native-ui/pixel-file-arm'/hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()[:20]
 src=out/'private-build';src.mkdir(parents=True,exist_ok=False)
 for p in paths:(src/p.name).write_bytes(p.read_bytes())
 (src/'libc.so').write_bytes(LIBC.read_bytes())
 env=dict(os.environ,TEMP=str(src),TMP=str(src),ZIG_LOCAL_CACHE_DIR=str(src/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(src/'zig-global'))
 commands=[]
 def run(cmd):
  commands.append(list(map(str,cmd)));r=subprocess.run(commands[-1],env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=120);(out/('command-%d.log'%len(commands))).write_text(r.stdout+r.stderr,encoding='utf8');assert not r.returncode,r.stdout+r.stderr
 run([ZIG,'cc','-target','aarch64-linux-gnu','-std=c11','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fPIC','-fno-builtin','-fno-stack-protector','-mno-outline-atomics','-D__errno_location=__errno','-c',src/'pixel_file_io.c','-o',src/'io.o'])
 binary=out/'pixel-file-io.so';run([ZIG,'ld.lld','-m','aarch64elf','-shared','--no-undefined','--allow-shlib-undefined','-z','noexecstack','--strip-all',src/'io.o',src/'libc.so','-o',binary])
 elf=FirmwareElf(binary.read_bytes()).elf;imports={s.name for s in elf.get_section_by_name('.dynsym').iter_symbols() if s.name and s['st_shndx']=='SHN_UNDEF'}
 needed=[t.needed for t in elf.get_section_by_name('.dynamic').iter_tags() if t.entry.d_tag=='DT_NEEDED'];assert needed==['libc.so'],needed
 assert all(sha(p)==identity[p.name] for p in paths)
 (out/'report.json').write_text(json.dumps(dict(identity=identity,artifact_sha256=sha(binary),imports=sorted(imports),needed=needed,commands=commands,linked=True,arm_executed=False,qt_attached=False,hardware_operations=0),ensure_ascii=False,indent=2),encoding='utf8');print(out);print('BARE_BIONIC_IO_LINK_PASS',sorted(imports))
if __name__=='__main__':main()
