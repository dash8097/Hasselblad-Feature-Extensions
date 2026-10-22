"""像素拍摄已发出后，只读验收最后同一任务；不补发快门。"""
import argparse,json,re,sys,time
from pathlib import Path
from package_ui import ROOT,OUT,PACKAGE,PACKAGE_ID,transport_lock,read_status
from package_pixel_shot import task_receipt,completion

def main():
 assert sys.version_info[:2]==(3,11) and Path.cwd().resolve()==ROOT
 p=argparse.ArgumentParser();p.add_argument('tag');p.add_argument('--capture-case',required=True);p.add_argument('--bit',type=int,choices=(0,1),required=True);p.add_argument('--frames',type=int,choices=(6,16),required=True);p.add_argument('--keep',type=int,choices=(0,1),required=True);a=p.parse_args()
 assert all(re.fullmatch('[A-Za-z0-9_-]+',v) for v in (a.tag,a.capture_case))
 target=OUT/(a.tag+'.json');assert not target.exists()
 actions=[json.loads(x) for x in (OUT/'capture-actions.jsonl').read_text().splitlines()]
 sent=[x for x in actions if x.get('package')==PACKAGE_ID and x.get('case')==a.capture_case]
 assert len(sent)==1 and sent[0]==actions[-1] and sent[0]['action']=='pixel-full-shutter-ui'
 action=sent[0];session=action['session'];ack=action['ack'];assert re.fullmatch('[0-9a-f]{8}',session) and ack>0
 sys.path.insert(0,str(PACKAGE/'engine'));import windows_engine
 with transport_lock():
  w=windows_engine.Transport(PACKAGE);i=windows_engine.mac_patch.Installer(PACKAGE,run_batch=w.batch);one=lambda c:w.batch([c])[0].strip()
  s=read_status(one);receipt=task_receipt(one)
  r=dict(package=PACKAGE_ID,time=time.time(),capture_case=a.capture_case,session=session,ack=ack,input_sent=False,photo_contents_read=False,task_receipt=receipt)
  checkpoint=OUT/(a.capture_case+'.json');r['baseline_receipt_available']=checkpoint.exists()
  if checkpoint.exists():
   prior=json.loads(checkpoint.read_text(encoding='utf8'));assert prior['package']==PACKAGE_ID and prior['session']==session and prior['ack']==ack
   assert prior['prior_task_receipt']!=receipt,'Receipt did not change after this task intent'
  r['status']={k:s.get(k) for k in ('session','ack','stage','busy','ready','prepared','error','frames','work_done','work_total')}
  for group,key in [('camera','exposure_status'),('storage','storage_status'),('error','active_error'),('camera','image_bit_depth')]:r[key]=i.property(group,key)
  r['mount']=one('awk \'$2=="/system"{print $4}\' /proc/mounts')
  published=bool(re.fullmatch('native-album-ui-'+session+'-[1-9][0-9]*',receipt.get('job_id',''))) and all(receipt.get(k)==v for k,v in dict(event='runner_exit',exit='0',signal='0',restored_marker='1',memory_failed_marker='0',album_verified='1').items())
  rearmed=True
  if a.keep:
   h=json.loads(one('cat /tmp/x2d-pixel-overclock/shutter-status.json'));r['rearmed_shutter']=h;arm=h.get('arm','').split()
   rearmed=h.get('available') is True and not h.get('pending') and len(arm)==8 and arm[:4]==[('PO3' if a.frames==6 else 'PO4'),session,str(ack+1),str(a.frames)] and arm[5:]==[str(a.keep),'START','END']
  r['passed']=bool(s.get('session')==session and completion(s,ack,a.frames,a.keep) and s.get('work_done')==s.get('work_total')==(17 if a.frames==6 else 55) and published and rearmed and r['exposure_status']==r['storage_status']=='0' and r['active_error']=='false' and r['image_bit_depth']==str(a.bit) and r['mount'].split(',')[0]=='ro')
 target.write_text(json.dumps(r,ensure_ascii=False,indent=2)+'\n',encoding='utf8');print(json.dumps(r,ensure_ascii=False));assert r['passed'],'Same task not accepted; no repeat shutter'

if __name__=='__main__':main()
