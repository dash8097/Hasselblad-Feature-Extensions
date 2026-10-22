"""原样 Bionic 引闪后台的关闭态协议测试；无相机、USB、网络及主机目录透传。"""
from pathlib import Path
import sys,os,io,json,hashlib,subprocess,shutil,argparse
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[2]
sys.path.insert(0,str(ROOT/'x2d/CodeTests/installer_hardening/camera_native'))
import checker_linux_env as linux
sys.path.insert(0,str(ROOT/'x2d/tools'))
from firmware_image import system_file
from elftools.elf.elffile import ELFFile
def sha(b):return hashlib.sha256(b).hexdigest()
def main():
 p=argparse.ArgumentParser();p.add_argument('report',type=Path);a=p.parse_args();report=json.loads(a.report.read_text())
 inputs={'test_native_service.c':(HERE/'test_native_service.c').read_bytes(),'native_parent_probe.c':(HERE/'native_parent_probe.c').read_bytes(),'esflash_protocol.h':(ROOT/'x2d/CodeTests/installer_hardening/native_ui/esflash_protocol.h').read_bytes()}
 identity=dict(report=sha(a.report.read_bytes()),sources={n:sha(b) for n,b in inputs.items()},builder=sha(Path(__file__).read_bytes()))
 out=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/esflash-check'/sha(json.dumps(identity,sort_keys=True).encode())[:20];out.mkdir(parents=True,exist_ok=False)
 for n,b in inputs.items():(out/n).write_bytes(b)
 env=dict(os.environ,TEMP=str(out),TMP=str(out),ZIG_LOCAL_CACHE_DIR=str(out/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(out/'zig-global'))
 r=subprocess.run([str(linux.ZIG),'cc','-target','aarch64-linux-musl','-static','-O2','-Wall','-Wextra','-Werror',str(out/'test_native_service.c'),'-o',str(out/'init')],env=env,capture_output=True,text=True,timeout=180);assert not r.returncode,r.stderr
 factory={};pending=['/bin/linker64','/lib64/libc.so','/lib64/libcrypto.so','/lib64/libdl.so']
 while pending:
  name=pending.pop()
  if name in factory:continue
  b=system_file(name);factory[name]=b;dynamic=ELFFile(io.BytesIO(b)).get_section_by_name('.dynamic')
  if dynamic:pending+=['/lib64/'+t.needed for t in dynamic.iter_tags() if t.entry.d_tag=='DT_NEEDED']
 for n in ('libc.so','libdl.so'):(out/n).write_bytes(factory['/lib64/'+n])
 probe=out/'parent-probe'
 obj=out/'parent-probe.o'
 r=subprocess.run([str(linux.ZIG),'cc','-target','aarch64-linux-gnu','-O2','-fPIE','-fno-stack-protector','-c',str(out/'native_parent_probe.c'),'-o',str(obj)],env=env,capture_output=True,text=True,timeout=180);assert not r.returncode,r.stderr
 r=subprocess.run([str(linux.ZIG),'ld.lld','-m','aarch64elf','-pie','--dynamic-linker','/system/bin/linker64','-e','_start','--no-undefined','--allow-shlib-undefined',str(obj),str(out/'libc.so'),str(out/'libdl.so'),'-o',str(probe)],env=env,capture_output=True,text=True,timeout=180);assert not r.returncode,r.stderr
 probe_data=probe.read_bytes();assert len(probe_data)<0x200000;probe_data+=b'\0'*(0x200000-len(probe_data))
 entries=[(n,b'',0o40755) for n in ['dev','proc','system','system/bin','system/lib64']]+[('init',(out/'init').read_bytes(),0o100755),('system/bin/camera-gui',(out/'init').read_bytes(),0o100755),('system/bin/camera-service',system_file('/bin/camera-service'),0o100400),('system/bin/flash-helper-probe',probe_data,0o100755)]
 entries += [('system'+n,b,0o100755) for n,b in factory.items()]
 for target in ('/system/bin/x2d-esflash','/system/lib64/libx2d_esflash_hotshoe.so'):
  row=report['targets'][target];b=(ROOT/row['source']).read_bytes();assert sha(b)==row['sha256'];entries.append((target[1:],b,0o100755))
 (out/'initramfs.cpio').write_bytes(linux.cpio(entries))
 command=[str(linux.DEPS/'qemu/qemu-system-aarch64.exe'),'-machine','virt','-cpu','cortex-a53','-m','256','-smp','1','-nic','none','-display','none','-monitor','none','-serial','stdio','-no-reboot','-kernel',str(linux.DEPS/'vmlinuz-virt'),'-initrd',str(out/'initramfs.cpio'),'-append','console=ttyAMA0 rdinit=/init panic=-1 quiet']
 r=subprocess.run(command,capture_output=True,text=True,timeout=45);(out/'console.txt').write_text(r.stdout+r.stderr);print(r.stdout);passed='FLASH_SERVICE_PASSED' in r.stdout and 'FLASH_SERVICE_FAILED' not in r.stdout and r.stdout.count('NATIVE_PARENT_LOADER_REJECTED_UNTRUSTED_HOST')==2
 (out/'report.json').write_text(json.dumps(dict(identity=identity,passed=passed,exact_backend=True,enabled=False,camera_requests=0,hardware_verified=False,factory_sha256={n:sha(b) for n,b in factory.items()}),indent=2));assert passed;print(out)
if __name__=='__main__':main()
