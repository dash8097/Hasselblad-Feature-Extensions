"""从仓库根目录运行的完整双档构建；不连接相机、不发布。先按 README 准备冻结输入。"""
from pathlib import Path
import subprocess,sys,json,os
ROOT=Path.cwd();R7=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7';CODE=ROOT/'x2d/CodeTests/installer_hardening';OUT=R7/'developer-delivery-build';OUT.mkdir(exist_ok=True)
def read(p):return json.loads(Path(p).read_text(encoding='utf8'))
def run(name,script,*args):
 print('START '+name,flush=True)
 r=subprocess.run([sys.executable,'-X','utf8','-B',str(script),*map(str,args)],capture_output=True,text=True,encoding='utf8',errors='replace',timeout=3600,env=dict(os.environ,PYTHONDONTWRITEBYTECODE='1'))
 (OUT/(name+'.log')).write_text(r.stdout+r.stderr,encoding='utf8')
 if r.returncode:raise RuntimeError(name+' failed: '+(r.stdout+r.stderr)[-3000:])
 print('PASS '+name,flush=True);return r.stdout.strip().splitlines()[-1]
def jr(name,script,*args):return json.loads(run(name,script,*args))
def save(): (OUT/'state.json').write_text(json.dumps(state,indent=2),encoding='utf8')
ui=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/native-ui/arm-bridge/c81473bb3583613bfdf8';assert read(ui/'build-proof.json')['source_matches_live_at_finish']
state={'ui':str(ui),'camera_operations':0};save()
contracts={}
for mask,suffix in [(248,'migration-acceptance-basic'),(255,'migration-acceptance')]:
 contracts[mask]=ui/suffix/'native-ui-contract.json'
 if not contracts[mask].exists():run('seal-'+str(mask),CODE/'native_ui/seal_native_ui_contract.py',ui/'build-proof.json','--feature-mask',mask)
flows={};manifests={}
for mask in (248,255):
 label=str(mask)
 run('runtime-'+label,ROOT/'x2d/CodeTests/raw_stack_continuous/build_runtime.py','--native-ui-contract',contracts[mask])
 binding=read(ROOT/'x2d/outputs/4.2.0/raw-stack/continuous-candidate/runtime/candidate-binding.json');proof=ROOT/binding['build_proof'];flow=read(proof);flows[mask]=proof.parent.name
 reports=[]
 for report in (R7/'checker-ui').glob('memory-full-*/report.json'):
  result=read(report)
  if result.get('passed') and result.get('exact_artifact_sha256')==flow['artifact']['sha256'] and all(not (CODE/'camera_native'/n).is_file() or __import__('hashlib').sha256((CODE/'camera_native'/n).read_bytes()).hexdigest()==v for n,v in result.get('source_sha256',{}).items()):reports.append(report)
 if reports:
  assert len(reports)==1
  print('REUSE validated memory-'+label+' '+str(reports[0]),flush=True)
 else:run('memory-'+label,CODE/'camera_native/checker_memory_full_linux.py','--revision',flows[mask],'--sha',flow['artifact']['sha256'],'--runtime-root',ROOT/'x2d/outputs/4.2.0/raw-stack/continuous-candidate/runtime')
 delivery=jr('continuous-'+label,ROOT/'x2d/CodeTests/raw_stack_continuous/prepare_candidate.py');manifests[mask]=Path(delivery['directory'])/'manifest.json';state['runtime-'+label]={'revision':flows[mask],'manifest':str(manifests[mask])};save()
def payload(label,mask,legacy):
 args=['--esflash-report',R7/'esflash/81a013378350c59e128d/report.json','--runtime-revision',flows[mask],'--feature-mask',mask,'--native-ui-contract',contracts[mask],'--continuous-manifest',manifests[mask]]
 for old in legacy:args+=['--legacy-payload',old]
 return Path(jr(label,CODE/'camera_native/build_candidate_payload.py',*args)['directory'])
old=[ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/native-payload/revisions/a8fe68c035db7dbe3a0c',ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/native-payload/revisions/2a1ca0963f7d51e147e4',R7/'native-payload/revisions/85c0bb0f931acc78c224',R7/'native-payload/revisions/244e9d61ecf754065122',R7/'native-payload/revisions/fcb7f9c1436d68ef1795',R7/'native-payload/revisions/ce2ec3ea5e79d69b55d0',R7/'native-payload/revisions/6344614820e49e1367dd',R7/'native-payload/revisions/669f39dd33dd9c22bb30',R7/'native-payload/revisions/1733d7d45638ab70b4e4',R7/'native-payload/revisions/ecbaf4706020b048d0c5',R7/'native-payload/revisions/e6852aadb7cdd7600af0',R7/'native-payload/revisions/3a09baa851c275cefdca',R7/'native-payload/revisions/16c8f7d9717dc6de005c',R7/'native-payload/revisions/8ae40a1f7eca5e734df6',R7/'native-payload/revisions/b2846120d6c27e72e16f',R7/'native-payload/revisions/61199d8409f6cbc100e9',R7/'native-payload/revisions/351f06472dce475249d0',R7/'native-payload/revisions/9dc93e9d821cca12b2cd',R7/'native-payload/revisions/8d3a9ab8a4dc72f5432f',R7/'native-payload/revisions/a9cc49a5bc84641721ea',R7/'native-payload/revisions/116ea758b2e2869212bf',R7/'native-payload/revisions/c43600ac2c59acc49d2b',R7/'native-payload/revisions/28bbb6d061ba7b6861df',R7/'native-payload/revisions/0c747900073002d79ed1',R7/'native-payload/revisions/76a94a81fee4bcad95c8',R7/'native-payload/revisions/f513da1c6d316a2169cc',R7/'native-payload/revisions/fc1253934b48609a9b56',R7/'native-payload/revisions/b1b66354e9118976c60a']
old += [R7/'native-payload/revisions/81f2394a45ccf59bd2c8', R7/'native-payload/revisions/c982b1ff17101d40f4ac']
old += [ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/native-payload/revisions/f8ae2551f555b340fd99', ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/native-payload/revisions/bbaef0bb2a71681995a0']
old += [ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/native-payload/revisions/4430c4f6235c61c4576c', ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/native-payload/revisions/8c0a88c36e77c5c7c693']
basic0=payload('payload-basic-initial',248,old)
advanced=payload('payload-advanced',255,[basic0,*old])
basic=payload('payload-basic',248,[advanced,*old]);setups={}
state.update(advanced=str(advanced),basic=str(basic));save()
for mask,p in [(248,basic),(255,advanced)]:
 r=jr('setup-'+str(mask),CODE/'camera_native/build_flow.py','--profile','setup','--hide-literals','--payload',p,'--native-ui-contract',contracts[mask]);d=Path(r.get('build_directory',r.get('reused_verified_build')));setups[mask]=d/'artifacts/x2d-native-flow'
 run('flash-exit-'+str(mask),CODE/'camera_native/test_flash_uninstall.py',d.name)
 run('cleanup-'+str(mask),CODE/'camera_native/test_data_cleanup_linux.py',d.name);state['setup-'+str(mask)]=str(setups[mask]);save()
args=['--payload',advanced,'--setup',setups[255],'--basic-payload',basic,'--basic-setup',setups[248]]
previous_args=[]
for row in read(CODE/'camera_native/legacy_cleanup_helpers.json'):
 previous_args+=['--previous-setup',ROOT/row['path']]
args+=previous_args
dual=jr('dual',CODE/'native_host/build_dual_clients.py',*args);state['dual']=dual;save()
run('package-check',CODE/'native_host/check_profile_package.py',dual['results']['windows']['assembled'],*previous_args)
state['delivery']=jr('delivery',CODE/'native_host/package_dual_candidates.py','--dual-build',dual['directory'],'--payload',advanced);save();print(json.dumps(state['delivery']),flush=True)
