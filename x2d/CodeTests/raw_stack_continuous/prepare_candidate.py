"""封存装机测试候选及原生依赖；不是正式R7安装包，不操作相机。"""
from pathlib import Path
import hashlib,io,json,tarfile,sys
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[2]
BASE=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/native-payload/revisions/85990aee97865310317d'
OUT=ROOT/'x2d/outputs/4.2.0/raw-stack/continuous-candidate'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):return json.loads(p.read_text(encoding='utf8'))
def main():
 assert Path.cwd().resolve()==ROOT
 binding=read(OUT/'runtime/candidate-binding.json');flow_path=ROOT/binding['build_proof'];flow=read(flow_path)
 sys.path.insert(0,str(ROOT/'x2d/CodeTests/installer_hardening/camera_native'))
 from native_worker_contract import validated_worker
 from af_speed_contract import validated_af_speed
 af_speed,af_proof,af_checks=validated_af_speed(ROOT)
 worker,worker_proof,worker_check=validated_worker(ROOT)
 capture=read(OUT/'capture/latest-build.json');ui=flow['build_identity']['native_ui_contract'];targets=read(BASE/'manifest.json')['targets']
 checks={}
 memory=read(OUT/'memory-gate/latest-build.json')
 full=[]
 for p in (ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/checker-ui').glob('memory-full-*/report.json'):
  r=read(p)
  if not r.get('passed') or r.get('exact_artifact_sha256')!=flow['artifact']['sha256']:continue
  sources=r.get('source_sha256',{})
  wanted=['native_memory_runner.cpp','native_memory_request.hpp','native_common.cpp','checker_memory_full_adapter.cpp','checker_memory_full_leaf.c','checker_memory_full_init.c']
  if all(sources.get(n)==sha(ROOT/'x2d/CodeTests/installer_hardening/camera_native'/n) for n in wanted):full.append(p)
 assert len(full)==1,('matching memory runner verification required',full)
 checks[str(full[0].relative_to(ROOT))]=sha(full[0])
 for folder,key in [('dma-check','source_sha256'),('checks','source_sha256'),('cpu-allocation-check','source_sha256'),('runner-check','source_sha256'),('publication-check','source_sha256'),('startup-check','source_sha256'),('memory-gate-check','source_sha256')]:
  valid=[]
  for p in (OUT/folder).glob('*/report.json'):
   r=read(p)
   if not r.get('passed'):continue
   hashes=r[key]
   def source(n):
    if '/' in n or '\\' in n:return ROOT/n
    if n=='receipt_sha256.h':return ROOT/'x2d/CodeTests/pixel_shift_rgb/receipt_sha256.h'
    return HERE/n if (HERE/n).is_file() else ROOT/'x2d/CodeTests/installer_hardening/camera_native'/n
   if all(sha(source(n))==v for n,v in hashes.items()):valid.append(p)
  assert len(valid)==1,(folder,valid)
  checks[str(valid[0].relative_to(ROOT))]=sha(valid[0])
 overrides={'p42-libx2d_private_memory_capture.so':ROOT/memory['artifact']['path'],'p00-libx2d_raw_stack.so':ROOT/capture['artifact']['path'],'p122-x2d-native-flow':flow_path.parent/'artifacts/x2d-native-flow','p18-libx2d_native_menu.so':ROOT/ui['artifact']}
 overrides['p09-libx2d_af_base0.so']=af_speed
 for proof in [af_proof,*af_checks]:checks[str(proof.relative_to(ROOT))]=sha(proof)
 assert sha(overrides['p00-libx2d_raw_stack.so'])==binding['capture_sha256']
 assert sha(overrides['p122-x2d-native-flow'])==flow['artifact']['sha256']
 assert sha(overrides['p18-libx2d_native_menu.so'])==ui['module_sha256']
 assert sha(overrides['p42-libx2d_private_memory_capture.so'])==memory['artifact']['sha256']
 quarter=read(OUT/'quarter/latest-build.json')
 quarter_proof=read(ROOT/quarter['proof']);quarter_check=read(ROOT/quarter['validation'])
 assert quarter_check['passed'] and quarter_check['module_sha256']==quarter['artifact']['sha256']==quarter_proof['file_sha256']
 for n,h in quarter_check['source_hashes'].items():assert sha(ROOT/'x2d/CodeTests/pixel_shift_rgb'/n)==h,n
 overrides['p66-libx2d_quarter_native.so']=ROOT/quarter['artifact']['path']
 assert sha(overrides['p66-libx2d_quarter_native.so'])==quarter['artifact']['sha256']
 checks[quarter['validation']]=sha(ROOT/quarter['validation'])
 checks[quarter['proof']]=sha(ROOT/quarter['proof'])
 codec=ROOT/quarter_proof['codec_proof'];assert sha(codec)==quarter_proof['codec_proof_sha256']
 checks[str(codec.relative_to(ROOT))]=sha(codec)
 software=read(ROOT/quarter['software_validation']);assert software['passed'] and software['real_codec']
 for n,h in software['source_hashes'].items():assert sha(ROOT/'x2d/CodeTests/pixel_shift_rgb'/n)==h,n
 checks[quarter['software_validation']]=sha(ROOT/quarter['software_validation'])
 arm=read(ROOT/quarter['arm_validation']);assert arm['passed'] and arm['arm_neon'] and arm['codec_proof_sha256']==sha(codec)
 for n,h in arm['source_hashes'].items():assert sha(ROOT/'x2d/CodeTests/pixel_shift_rgb'/n)==h,n
 checks[quarter['arm_validation']]=sha(ROOT/quarter['arm_validation'])


 overrides['p114-libx2d_private_memory_capture.so']=overrides['p42-libx2d_private_memory_capture.so']
 worker_targets=[r for r in targets if r['path']=='/system/bin/x2d-pixel-overclock/worker'];assert len(worker_targets)==1
 overrides[worker_targets[0]['name']]=worker
 for proof in (worker_proof,worker_check):checks[str(proof.relative_to(ROOT))]=sha(proof)
 assert sha(HERE/'memory_request_gate.h')==memory['identity']['sources']['memory_request_gate.h']
 assert sha(HERE/'build_memory_gate.py')==memory['identity']['builder']
 files=[]
 for t in targets:
  p=overrides.get(t['name'],BASE/'private-payload/install'/t['name'])
  if t['name'] not in overrides:assert sha(p)==t['candidate_sha256']
  assert not p.name.endswith(('.sh','.py','.ps1','.js','.qml'))
  files.append(dict(name=t['name'],path=t['path'],mode=t['mode'],sha256=sha(p),bytes=p.stat().st_size,source=str(p.relative_to(ROOT))))
 identity=dict(files=files,checks=checks,builder=sha(Path(__file__)),worker_contract_sha256=sha(ROOT/'x2d/CodeTests/installer_hardening/camera_native/native_worker_contract.py'),flow_proof_sha256=sha(flow_path),firmware='4.2.0')
 revision=hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()[:16]
 out=OUT/'delivery'/revision;out.mkdir(parents=True,exist_ok=True)
 archive=out/'archive.gz'
 with tarfile.open(archive,'w:gz',compresslevel=6) as tar:
  for f in files:
   data=(ROOT/f['source']).read_bytes();info=tarfile.TarInfo(f['name']);info.size=len(data);info.mode=int(f['mode'],8);info.mtime=0
   tar.addfile(info,io.BytesIO(data))
 assert archive.stat().st_size<16777216
 manifest=dict(identity=identity,device_directory='/data/local/tmp/x2d-burst-'+revision[:8],archive_sha256=sha(archive),hardware_tested=False,installed=False,official_release=False)
 (out/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
 print(json.dumps(dict(directory=str(out),archive_bytes=archive.stat().st_size,files=len(files),sha256=sha(archive)),ensure_ascii=False))
if __name__=='__main__':main()
