"""精确门禁与显式同源测试manifest分离；不执行原厂服务/原厂codec。"""
import argparse,hashlib,json,os,re,shutil,subprocess,sys
import checker_linux_env as base
ROOT=base.ROOT;R7=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7'
def main():
 p=argparse.ArgumentParser();p.add_argument('--revision',required=True);p.add_argument('--sha',required=True);p.add_argument('--model',action='store_true');p.add_argument('--mapping',action='store_true');p.add_argument('--restart-failure',action='store_true');a=p.parse_args();assert re.fullmatch('[0-9a-f]{20}',a.revision) and re.fullmatch('[0-9a-f]{64}',a.sha);assert not (a.mapping or a.restart_failure) or a.model;assert not(a.mapping and a.restart_failure)
 assert ROOT==base.Path.cwd().resolve();rev=R7/'native-flow/revisions'/a.revision;out=R7/('checker-ui/install-'+a.sha[:8])/('restart-model' if a.restart_failure else 'mapping-model' if a.mapping else 'model' if a.model else 'exact');out.mkdir(parents=True,exist_ok=True);src=out/'source';src.mkdir(exist_ok=True)
 proof=json.loads((rev/'build-proof.json').read_text(encoding='utf-8'));artifact=rev/'artifacts/x2d-native-flow';assert base.sha(artifact)==a.sha
 for n,h in proof['source_sha256'].items():shutil.copyfile(rev/'private-build/source'/n,src/n);assert base.sha(src/n)==h
 header=(src/'native_install_tables.hpp').read_text(encoding='utf-8');rows=re.findall(r'\{"(p\d+-[^\"]+)","([^\"]+)","([^\"]+)","([0-9a-f]{64})",(0[0-7]+)\}',header);assert len(rows)==112
 guards=re.findall(r'\{"(/system/[^\"]+)","([0-9a-f]{64})"\}',header);assert len(guards)==5
 sys.path.insert(0,str(ROOT/'x2d/tools'));from firmware_image import system_file
 entries=[(d,b'',0o40755) for d in ['dev','proc','tmp','data','data/local','data/local/tmp','data/local/tmp/x2d-rs1','bin','fake','system','fixtures','payload']];fx=[];fixtures={}
 for i,(path,digest) in enumerate(guards+[(path,original) for n,path,original,digest,mode in rows if original!='ABSENT']):
  data=system_file(path.removeprefix('/system'));assert hashlib.sha256(data).hexdigest()==digest;n='firmware'+str(i);entries.append(('fixtures/'+n,data,0o100400));fx.append((n,path));fixtures[path]=digest
 for n in ['linker64','libc.so','libdl.so']:
  data=system_file(('/bin/' if n=='linker64' else '/lib64/')+n);entries.append(('fixtures/'+n,data,0o100400));fixtures[n]=hashlib.sha256(data).hexdigest()
 payloadroot=ROOT/'x2d/outputs/4.2.0/installer-hardening-r3/camera-iso-candidate-v4/public-install';payload={}
 for n,path,original,digest,mode in rows:data=(payloadroot/n).read_bytes();assert hashlib.sha256(data).hexdigest()==digest;payload[n]=data
 payload['check-raw-stack-arm']=(payloadroot/'check-raw-stack-arm').read_bytes();assert hashlib.sha256(payload['check-raw-stack-arm']).hexdigest() in header
 probe=R7/'install-probe/revisions/092516980d4ea967eb63/artifacts/install-probe';payload['install-probe']=probe.read_bytes();assert base.sha(probe) in header
 fixture_header='static const struct {const char*src,*dst;} fixtures[]={'+','.join('{"/fixtures/'+n+'","'+p+'"}' for n,p in fx)+'};\n'
 fixture_header+='static const char*inputs[]={'+','.join(json.dumps(n) for n in payload)+'};\n';(src/'checker_install_fixtures.h').write_text(fixture_header)
 env=dict(os.environ,TEMP=str(out),TMP=str(out),ZIG_LOCAL_CACHE_DIR=str(out/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(out/'zig-global'))
 def cc(args):
  r=subprocess.run([str(base.ZIG),*map(str,args)],env=env,capture_output=True,text=True,timeout=240);assert not r.returncode,r.stderr
 for name in ['init','child']:
  f=src/('checker_install_'+name+'.c');shutil.copyfile(base.HERE/f.name,f);cc(['cc','-D_GNU_SOURCE','-std=c11','-O2','-Wall','-Wextra','-Werror','-target','aarch64-linux-musl','-static','-s',*(['-DINSTALL_RESTART_FAULT'] if a.restart_failure else ['-DINSTALL_MAP_FAULT'] if a.mapping else ['-DINSTALL_MODEL'] if a.model else []),'-I',src,f,'-o',out/name])
 substitutions={};binary=artifact
 if a.model:
  empty=src/'checker_empty.c';empty.write_text('__attribute__((constructor)) static void empty_ctor(void) {}\nint checker_empty(void){return 0;}\n');cc(['cc','-O2','-target','aarch64-linux-musl','-shared','-nostdlib','-fno-stack-protector',empty,'-o',out/'empty.so'])
  for n,data in [('p00-libx2d_raw_stack.so',(out/'empty.so').read_bytes()),('p04-render-probe',(out/'child').read_bytes()),('check-raw-stack-arm',(out/'child').read_bytes())]:
   old=hashlib.sha256(payload[n]).hexdigest();new=hashlib.sha256(data).hexdigest();assert header.count(old)==1;header=header.replace(old,new);substitutions[n]={'old':old,'new':new};payload[n]=data
  (src/'native_install_tables.hpp').write_text(header,encoding='utf-8');objs=[];common=['-target','aarch64-linux-musl','-Oz','-Wall','-Wextra','-Werror','-fvisibility=hidden','-ffunction-sections','-fdata-sections','-fstack-protector-strong']
  for n in proof['source_sha256']:
   if not n.endswith(('.c','.cpp')):continue
   obj=out/(n+'.o');objs.append(obj);cpp=n.endswith('.cpp');cc(['c++' if cpp else 'cc',*common,'-std=c++17' if cpp else '-std=c11','-DCN_FULL_FLOW=1','-c',src/n,'-o',obj])
  binary=out/'flow-test-manifest';cc(['c++',*common,'-static','-Wl,--gc-sections','-s',*objs,'-o',binary])
 for n,data in payload.items():entries.append(('payload/'+n,data,0o100400))
 for n in ['bin/dbus-send','fake/camera-service','fake/camera-gui','fake/mapholder','fixtures/child']:entries.append((n,(out/'child').read_bytes(),0o100755))
 entries.extend([('flow',binary.read_bytes(),0o100755),('init',(out/'init').read_bytes(),0o100755)]);initrd=out/'initramfs.cpio';initrd.write_bytes(base.cpio(entries))
 args=[str(base.DEPS/'qemu/qemu-system-aarch64.exe'),'-machine','virt','-cpu','cortex-a53','-m','1024','-smp','2','-nic','none','-display','none','-monitor','none','-serial','stdio','-no-reboot','-kernel',str(base.DEPS/'vmlinuz-virt'),'-initrd',str(initrd),'-append','console=ttyAMA0 rdinit=/init panic=-1 quiet']
 r=subprocess.run(args,capture_output=True,text=True,timeout=240);(out/'console.txt').write_text(r.stdout+r.stderr,encoding='utf-8')
 report=dict(exact_artifact=not a.model,production_sha256=a.sha,tested_sha256=base.sha(binary),substitutions=substitutions,source_sha256={n:base.sha(src/n) for n in proof['source_sha256']},fixture_sha256=fixtures,checker_sha256={p.name:base.sha(p) for p in src.glob('checker_*')},command=args,stdout=r.stdout,stderr=r.stderr,passed=r.returncode==0 and 'INSTALL_CHECK_ALL_PASSED' in r.stdout,real_camera=False,network=False,usb=False,host_shared_directory=False,original_services_executed=False)
 (out/'report.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print(r.stdout);assert report['passed']
if __name__=='__main__':main()
