from pathlib import Path
import sys,json,os,subprocess,hashlib,shutil
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE.parent/'installer_hardening/camera_native'))
import checker_linux_env as b
names=['native_publish.cpp','native_common.hpp','checker_publish_startup.cpp']
source=lambda n:HERE/n if (HERE/n).is_file() else b.HERE/n
hashes={n:b.sha(source(n)) for n in names}
out=b.ROOT/'x2d/outputs/4.2.0/raw-stack/continuous-candidate/startup-check'/hashlib.sha256(json.dumps(hashes,sort_keys=True).encode()).hexdigest()[:20]
out.mkdir(parents=True,exist_ok=True)
for n in names:shutil.copyfile(source(n),out/n)
env=dict(os.environ,TEMP=str(out),TMP=str(out),ZIG_LOCAL_CACHE_DIR=str(out/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(out/'zig-global'))
cmd=[str(b.ZIG),'c++','-std=c++17','-O2','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-target','aarch64-linux-musl','-static','-s',str(out/'checker_publish_startup.cpp'),'-o',str(out/'init')]
r=subprocess.run(cmd,env=env,capture_output=True,text=True,timeout=240);assert r.returncode==0,r.stderr
(out/'initramfs.cpio').write_bytes(b.cpio([('dev',b'',0o40755),('init',(out/'init').read_bytes(),0o100755)]))
cmd=[str(b.DEPS/'qemu/qemu-system-aarch64.exe'),'-machine','virt','-cpu','cortex-a53','-m','256','-nic','none','-display','none','-monitor','none','-serial','stdio','-no-reboot','-kernel',str(b.DEPS/'vmlinuz-virt'),'-initrd',str(out/'initramfs.cpio'),'-append','console=ttyAMA0 rdinit=/init panic=-1 quiet']
r=subprocess.run(cmd,capture_output=True,text=True,timeout=90)
(out/'console.txt').write_text(r.stdout+r.stderr,encoding='utf8');passed=r.returncode==0 and 'STARTUP_PASSED checks=6' in r.stdout
(out/'report.json').write_text(json.dumps(dict(passed=passed,source_sha256=hashes,cases=6,real_camera=False,dbus='explicit mock',production_wait_function=True),indent=2),encoding='utf8')
print(r.stdout);print(out);assert passed
