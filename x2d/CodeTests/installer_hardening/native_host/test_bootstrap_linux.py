"""引导 native commit 的真实文件系统/子进程模型；原厂 DBus 为纯只读替身。"""
from pathlib import Path
import hashlib,json,os,shutil,subprocess,sys,io,gzip
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[3]
sys.path.insert(0,str(HERE.parent/'camera_native'))
sys.path.insert(0,str(HERE.parent))
import checker_linux_env as linux
from native_bootstrap import landing_command
from native_network_upload import receiver_command
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
inputs={n:HERE/n for n in ('native_bootstrap.cpp','native_bootstrap.py','native_network_upload.py','test_bootstrap_linux.cpp','test_bootstrap_linux.py','test_native_bootstrap.py')}
inputs.update({n:HERE.parent/'camera_native'/n for n in ('native_common.hpp','native_common.cpp')});inputs['receipt_sha256.h']=HERE.parents[1]/'pixel_shift_rgb/receipt_sha256.h'
hashes={n:sha(p) for n,p in inputs.items()};revision=hashlib.sha256(json.dumps(hashes,sort_keys=True).encode()).hexdigest()[:20]
out=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/native-host/bootstrap'/revision;build=out/'private-build';build.mkdir(parents=True,exist_ok=True)
for n,p in inputs.items():shutil.copyfile(p,build/n)
assert all(sha(build/n)==h for n,h in hashes.items()),'source changed while freezing'
sys.path.insert(0,str(ROOT/'x2d/tools'))
import firmware_image as firmware
from elftools.elf.elffile import ELFFile
factory={};pending=['/bin/sh','/bin/toybox','/bin/linker64']
while pending:
    name=pending.pop()
    if name in factory:continue
    data=firmware.system_file(name);assert data[:4]==b'\x7fELF',name
    factory[name]=data;elf=ELFFile(io.BytesIO(data));dynamic=elf.get_section_by_name('.dynamic')
    if dynamic:
        pending += ['/lib64/'+tag.needed for tag in dynamic.iter_tags() if tag.entry.d_tag=='DT_NEEDED']
factory_hashes={n:hashlib.sha256(d).hexdigest() for n,d in factory.items()}
assert factory_hashes['/bin/toybox']=='999a0669ef654efbda54bc585c0f3c00643d1af61c985bfb7968cfd2f9aed840'
assert factory_hashes['/bin/sh']=='528507fdee38348abd5805016592e489925de84b3fec3b6fadee157e6c940f44'
(build/'landing_command.hpp').write_text('static const char*landing_command='+json.dumps(landing_command('/system/bin/x2d-12345678'))+';\nstatic const char*factory_files[]={'+','.join(json.dumps(n) for n in factory)+'};\n',encoding='ascii')
with (build/'landing_command.hpp').open('a', encoding='ascii') as f:
    f.write('static const char*network_command='+json.dumps(receiver_command('z12345678').replace('192.168.42.2','127.0.0.1'))+';\n')
env=dict(os.environ,TEMP=str(build),TMP=str(build),ZIG_LOCAL_CACHE_DIR=str(build/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(build/'zig-global'))
cmd=[str(linux.ZIG),'c++','-target','aarch64-linux-musl','-std=c++17','-O2','-Wall','-Wextra','-Werror','-static','-s','-ffunction-sections','-fdata-sections',str(build/'test_bootstrap_linux.cpp'),str(build/'native_common.cpp'),'-Wl,--gc-sections','-Dmount=test_mount','-o',str(build/'init')]
r=subprocess.run(cmd,env=env,capture_output=True,text=True,timeout=240);assert r.returncode==0,r.stdout+r.stderr
busybox=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/native-host/deps/busybox-armv6l-1.26.2';assert sha(busybox)=='bfeb2c8a28b3b94d3c911f012221179c1bbba8c7295544a00a69456f0818af02'
entries=[('dev',b'',0o40755),('busybox',busybox.read_bytes(),0o100755),('init',(build/'init').read_bytes(),0o100755),('factory',b'',0o40755),('factory/bin',b'',0o40755),('factory/lib64',b'',0o40755)]
entries += [('factory'+n,d,0o100755) for n,d in factory.items()]
known_seed=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/native-flow/revisions/ca601673b6cf9ba67a37/artifacts/x2d-native-flow'
assert sha(known_seed)=='a1815fcaec5200734d94f764a42f7320bd6ee8094767c4d19bf97f3c61863dfa'
entries.append(('known-seed',known_seed.read_bytes(),0o100755))
entries.append(('landing-gzip',gzip.compress(bytes(i%251 for i in range(1631680)),mtime=0),0o100400))
initrd=build/'initramfs.cpio';initrd.write_bytes(linux.cpio(entries))
cmd=[str(linux.DEPS/'qemu/qemu-system-aarch64.exe'),'-machine','virt','-cpu','cortex-a53','-m','256','-smp','1','-nic','none','-display','none','-monitor','none','-serial','stdio','-no-reboot','-kernel',str(linux.DEPS/'vmlinuz-virt'),'-initrd',str(initrd),'-append','console=ttyAMA0 rdinit=/init panic=-1 quiet']
r=subprocess.run(cmd,capture_output=True,text=True,timeout=90);(out/'console.txt').write_text(r.stdout+r.stderr,encoding='utf8');print(r.stdout);assert 'NATIVE_BOOTSTRAP_LINUX_PASSED' in r.stdout,r.stdout+r.stderr
(out/'report.json').write_text(json.dumps(dict(source_sha256=hashes,passed=True,hardware_calls=0,author_tests=True,independent_approval=False,factory_420_files_sha256=factory_hashes,landing_command=landing_command('/system/bin/x2d-12345678'),limitations=['DBus只读替身','Linux tmpfs挂载，不是真实相机','异常分支mount/cat/chmod替身；正常分支使用原厂4.2.0 sh/toybox及依赖'],test_injection='renameat2 retained failure once; production source unchanged'),ensure_ascii=False,indent=2)+'\n',encoding='utf8');print(out)
