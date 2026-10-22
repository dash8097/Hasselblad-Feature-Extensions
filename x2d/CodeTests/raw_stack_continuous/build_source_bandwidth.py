"""构建自有缓冲读取基准；无相机连接。"""
from pathlib import Path
import sys,os,subprocess,hashlib,json,gzip
ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'x2d/tools'))
from firmware_image import system_file,FirmwareElf
src=Path(__file__).with_name('check_source_bandwidth.c')
revision=hashlib.sha256(src.read_bytes()).hexdigest()[:16]
out=ROOT/'x2d/outputs/4.2.0/raw-stack/continuous-candidate/source-bandwidth'/revision
out.mkdir(parents=True,exist_ok=True)
(out/'source.c').write_bytes(src.read_bytes())
zig=ROOT/'.research-cache/x1d-1.25.0/toolchain/zig-windows-x86_64-0.13.0/zig.exe'
env=dict(os.environ,TEMP=str(out),TMP=str(out),ZIG_LOCAL_CACHE_DIR=str(out/'local'),ZIG_GLOBAL_CACHE_DIR=str(out/'global'))
libs=[]
for name in ('libc.so','libduml_hal.so'):
 p=out/name;p.write_bytes(system_file('/lib64/'+name));libs.append(p)
obj=out/'probe.o';binary=out/'probe'
cmds=[[zig,'cc','-target','aarch64-linux-gnu','-O2','-Wall','-Wextra','-Werror','-fPIE','-fno-stack-protector','-mno-outline-atomics','-c',src,'-o',obj],[zig,'ld.lld','-m','aarch64elf','-pie','--no-undefined','--allow-shlib-undefined','-z','noexecstack','-e','_start','--strip-all','--dynamic-linker=/system/bin/linker64',obj,*libs,'-o',binary]]
for c in cmds:subprocess.run(list(map(str,c)),env=env,check=True)
e=FirmwareElf(binary.read_bytes()).elf
assert not any(s['p_type']=='PT_TLS' for s in e.iter_segments())
compressed=out/'probe.gz';compressed.write_bytes(gzip.compress(binary.read_bytes(),mtime=0))
report=dict(revision=revision,source_sha256=hashlib.sha256(src.read_bytes()).hexdigest(),sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),gzip_sha256=hashlib.sha256(compressed.read_bytes()).hexdigest(),output=str(out),hardware_tested=False)
(out/'build.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report))
