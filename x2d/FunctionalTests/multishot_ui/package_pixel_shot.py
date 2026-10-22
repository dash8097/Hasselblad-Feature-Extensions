"""已由完整包UI准备的6/16帧像素超频：单次实体快门及同任务收尾验证。"""
import argparse,json,re,sys,time
from pathlib import Path
from package_ui import ROOT,OUT,PACKAGE,PACKAGE_ID,transport_lock,read_status

def task_receipt(one):
 # Control-only bounded archive section; never read image paths or metadata.
 text=one("if test -f /tmp/x2d-pixel-overclock/last-task.txt;then sed -n '/^\\[task.result\\]/,/^\\[/p' /tmp/x2d-pixel-overclock/last-task.txt;fi")
 keys=('protocol','job_id','event','last_observed_stage','end_monotonic_ms','exit','signal','restored_marker','memory_failed_marker','album_verified')
 return {k:v for line in text.splitlines() if '=' in line for k,v in [line.split('=',1)] if k in keys}

def completion(s,ack,frames,keep):
 if s.get('ack')!=ack or s.get('busy') is not False or s.get('ready') is not True or s.get('error') or s.get('frames')!=frames:return False
 if keep:return s.get('stage')=='idle' and s.get('prepared') is True and s.get('work_done')==s.get('work_total')==(17 if frames==6 else 55)
 return s.get('stage')=='complete' and not s.get('prepared')

def main():
 assert sys.version_info[:2]==(3,11) and Path.cwd().resolve()==ROOT
 ap=argparse.ArgumentParser();ap.add_argument('tag');ap.add_argument('--bit',type=int,choices=(0,1),required=True);ap.add_argument('--frames',type=int,choices=(6,16),required=True);ap.add_argument('--keep',type=int,choices=(0,1),required=True);a=ap.parse_args();assert re.fullmatch('[A-Za-z0-9_-]+',a.tag)
 target=OUT/(a.tag+'.json');assert not target.exists();sys.path.insert(0,str(PACKAGE/'engine'));import windows_engine
 r={'package':PACKAGE_ID,'time':time.time(),'bit':a.bit,'frames':a.frames,'keep':a.keep,'photo_contents_read':False,'observations':[]}
 with transport_lock():
  w=windows_engine.Transport(PACKAGE);i=windows_engine.mac_patch.Installer(PACKAGE,run_batch=w.batch);one=lambda c:w.batch([c])[0].strip()
  assert 'NATIVE_UI_READY_CURRENT_REQUEST' in one('/system/bin/x2d-native-flow ui-ready')
  s=read_status(one);h=json.loads(one('cat /tmp/x2d-pixel-overclock/shutter-status.json'));arm=h['arm'].split();assert s['ready'] and not s['busy'] and s['prepared'] and h['available'] and not h['pending']
  assert len(arm)==8 and arm[0]==('PO3' if a.frames==6 else 'PO4') and arm[1]==s['session'] and int(arm[3])==a.frames and int(arm[5])==a.keep and arm[6:]==['START','END']
  session=arm[1];ack=int(arm[2]);assert re.fullmatch('[0-9a-f]{8}',session) and ack>0;r.update(session=session,ack=ack)
  assert i.property('camera','image_bit_depth')==str(a.bit) and i.property('camera','exposure_status')=='0' and i.property('error','active_error')=='false'
  assert one('cat /tmp/x2d-pixel-overclock/quarter.txt')=='DISARMED' and one('cat /sys/class/input/event3/device/name')=='exposure_button'
  prior_receipt=task_receipt(one);r['prior_task_receipt']=prior_receipt;r['passed']=False;r['phase']='input_unconfirmed'
  # Persist intent and baseline before the only input. A lost USB ACK must not
  # erase task identity or allow this command to be run again as a fresh test.
  target.write_text(json.dumps(r,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
  with (OUT/'capture-actions.jsonl').open('a',encoding='utf8') as f:f.write(json.dumps(dict(package=PACKAGE_ID,case=a.tag,time=time.time(),action='pixel-full-shutter-ui',session=session,ack=ack,intent_before_input=True))+'\n')
  one('for e in "1 18 1" "0 0 0" w "1 18 0" "0 0 0";do if [ "$e" = w ];then sleep .08;else sendevent /dev/input/event3 $e;fi;done')
  r['phase']='observing'
  print('FULL_SHUTTER_UI_INPUT_SENT',session,ack,flush=True)
  end=time.monotonic()+600;previous=None
  while True:
   s=read_status(one);observed=s.get('ack',0);assert s.get('session')==session and observed in (ack-1,ack),'Task identity changed; no further input'
   sample={k:s.get(k) for k in ('ack','stage','busy','ready','error','frames','prepared','work_done','work_total','render_tiles','jpeg_tiles','jpeg_join_complete')};sample['active_error']=i.property('error','active_error');sample['exposure_status']=i.property('camera','exposure_status');sample['storage_status']=i.property('storage','storage_status')
   if sample!=previous:
    r['observations'].append(dict(time=time.time(),**sample));print(json.dumps(sample,ensure_ascii=False),flush=True);previous=sample
    target.write_text(json.dumps(r,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
   if sample['active_error']!='false' or (observed==ack and s.get('error')) or (completion(s,ack,a.frames,a.keep) and sample['exposure_status']==sample['storage_status']=='0'):break
   if time.monotonic()>=end:break
   time.sleep(1)
  r['status']=sample;r['mount']=one('awk \'$2=="/system"{print $4}\' /proc/mounts')
  receipt=task_receipt(one);r['task_receipt']=receipt
  published=receipt!=prior_receipt and re.fullmatch('native-album-ui-'+session+'-[1-9][0-9]*',receipt.get('job_id','')) and all(receipt.get(k)==v for k,v in dict(event='runner_exit',exit='0',signal='0',restored_marker='1',memory_failed_marker='0',album_verified='1').items())
  rearmed=True
  if a.keep:
   arm=json.loads(one('cat /tmp/x2d-pixel-overclock/shutter-status.json'));r['rearmed_shutter']=arm
   rearmed=arm.get('available') is True and not arm.get('pending') and arm.get('arm','').split()==[('PO3' if a.frames==6 else 'PO4'),session,str(ack+1),str(a.frames),h['arm'].split()[4],str(a.keep),'START','END']
  r['passed']=bool(completion(s,ack,a.frames,a.keep) and published and rearmed and sample['active_error']=='false' and sample['exposure_status']=='0' and i.property('storage','storage_status')=='0' and r['mount'].split(',')[0]=='ro')
  r['phase']='accepted' if r['passed'] else 'not_accepted'
  target.write_text(json.dumps(r,ensure_ascii=False,indent=2)+'\n',encoding='utf8');assert r['passed'],'Pixel task not accepted; no repeat shutter'
  print('PIXEL_COMPLETE_IDLE_VERIFIED',flush=True)
if __name__=='__main__':main()
