"""精确卸载ELF真实Linux回归；原厂文件仅不可执行hash输入。"""
import os,sys,json,hashlib,shutil,subprocess
import checker_linux_env as base
ROOT=base.ROOT;R7=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7'
REV=R7/'native-flow/revisions/b934bb42b0e3744a2969';SHA='01bf7a8a8fa33b7d801e68a1baa8c3cdf7d704c76de431e6dc455218e050dcce';OUT=R7/'checker-ui/uninstall-01bf7a8a'
def main():
 assert ROOT==base.Path.cwd().resolve();OUT.mkdir(parents=True,exist_ok=True);src=OUT/'source';src.mkdir(exist_ok=True)
 proof=json.loads((REV/'build-proof.json').read_text(encoding='utf-8'));artifact=REV/'artifacts/x2d-native-flow';assert base.sha(artifact)==SHA
 for n,h in proof['source_sha256'].items():shutil.copyfile(REV/'private-build/source'/n,src/n);assert base.sha(src/n)==h
 sys.path.insert(0,str(ROOT/'x2d/tools'));from firmware_image import system_file
 table=ROOT/'x2d/outputs/4.2.0/installer-hardening-r3/internal-candidates/windows-installer/uninstall'
 entries=[(d,b'',0o40755) for d in ['dev','proc','tmp','data','data/local','data/local/tmp','bin','fake','system','fixtures']];bindings={};fx=[];stocks=[]
 for i,line in enumerate((table/'guards.txt').read_text(encoding='utf-8').splitlines()):
  path,digest=line.split();data=system_file(path.removeprefix('/system'));assert hashlib.sha256(data).hexdigest()==digest;n='guard'+str(i);entries.append(('fixtures/'+n,data,0o100400));fx.append((n,path));bindings[path]=digest
 for line in (table/'targets.txt').read_text(encoding='utf-8').splitlines():
  id,path,original,*_=line.split()
  if original=='ABSENT':continue
  data=system_file(path.removeprefix('/system'));assert hashlib.sha256(data).hexdigest()==original;entries.append(('fixtures/stock-'+id,data,0o100400));stocks.append((id,path));bindings[path]=original
 header='static const struct {const char*src,*dst;} fixtures[]={'+','.join('{"/fixtures/'+n+'","'+p+'"}' for n,p in fx)+'};\n'
 header+='static const struct {const char*id,*src,*dst;} stocks[]={'+','.join('{"'+n+'","/fixtures/stock-'+n+'","'+p+'"}' for n,p in stocks)+'};\n';(src/'checker_uninstall_fixtures.h').write_text(header)
 env=dict(os.environ,TEMP=str(OUT),TMP=str(OUT),ZIG_LOCAL_CACHE_DIR=str(OUT/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(OUT/'zig-global'))
 for name in ['init','child']:
  f=src/('checker_uninstall_'+name+'.c');shutil.copyfile(base.HERE/f.name,f)
  p=subprocess.run([str(base.ZIG),'cc','-D_GNU_SOURCE','-std=c11','-O2','-Wall','-Wextra','-Werror','-target','aarch64-linux-musl','-static','-s','-I',str(src),str(f),'-o',str(OUT/name)],env=env,capture_output=True,text=True,timeout=180);assert not p.returncode,p.stderr
 for n in ['targets','guards']:entries.append(('fixtures/'+n+'.txt',(table/(n+'.txt')).read_bytes(),0o100400))
 for n in ['bin/dbus-send','fake/camera-service','fake/camera-gui','fake/camera-storage','fixtures/child']:entries.append((n,(OUT/'child').read_bytes(),0o100755))
 entries.extend([('flow',artifact.read_bytes(),0o100755),('init',(OUT/'init').read_bytes(),0o100755)]);initrd=OUT/'initramfs.cpio';initrd.write_bytes(base.cpio(entries))
 args=[str(base.DEPS/'qemu/qemu-system-aarch64.exe'),'-machine','virt','-cpu','cortex-a53','-m','1024','-smp','2','-nic','none','-display','none','-monitor','none','-serial','stdio','-no-reboot','-kernel',str(base.DEPS/'vmlinuz-virt'),'-initrd',str(initrd),'-append','console=ttyAMA0 rdinit=/init panic=-1 quiet']
 p=subprocess.run(args,capture_output=True,text=True,timeout=180);(OUT/'console.txt').write_text(p.stdout+p.stderr)
 report=dict(artifact_sha256=SHA,source_sha256=proof['source_sha256'],fixture_sha256=bindings,checker_sha256={p.name:base.sha(p) for p in src.glob('checker_*')},command=args,stdout=p.stdout,stderr=p.stderr,passed=p.returncode==0 and 'UNINSTALL_ALL_PASSED' in p.stdout,real_camera=False,network=False,usb=False,host_shared_directory=False,original_firmware_executed=False)
 (OUT/'report.json').write_text(json.dumps(report,indent=2));print(p.stdout);assert report['passed']
if __name__=='__main__':
 import argparse,re
 p=argparse.ArgumentParser();p.add_argument('--revision',default='b934bb42b0e3744a2969');p.add_argument('--sha',default=SHA);a=p.parse_args()
 assert re.fullmatch('[0-9a-f]{20}',a.revision) and re.fullmatch('[0-9a-f]{64}',a.sha)
 REV=R7/'native-flow/revisions'/a.revision;SHA=a.sha;OUT=R7/('checker-ui/uninstall-'+SHA[:8]);main()

