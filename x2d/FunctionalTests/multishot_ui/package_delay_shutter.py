"""已由实际UI准备的像素模式：只按一次原快门并等待倒计时，供UI取消用例。"""
import argparse,json,re,sys,time
from pathlib import Path
from package_ui import ROOT,OUT,PACKAGE,PACKAGE_ID,transport_lock,read_status
def main():
 assert sys.version_info[:2]==(3,11) and Path.cwd().resolve()==ROOT
 p=argparse.ArgumentParser();p.add_argument('tag');mode=p.add_mutually_exclusive_group(required=True);mode.add_argument('--frames',type=int,choices=(6,16));mode.add_argument('--method',type=int,choices=(0,1,2));p.add_argument('--bit',type=int,choices=(0,1),required=True);a=p.parse_args()
 assert re.fullmatch('[A-Za-z0-9_-]+',a.tag)
 target=OUT/(a.tag+'.json');assert not target.exists()
 journal=OUT/'capture-actions.jsonl'
 prior=[json.loads(line) for line in journal.read_text(encoding='utf8').splitlines()] if journal.exists() else []
 assert not any(v.get('package')==PACKAGE_ID and v.get('case',v.get('tag'))==a.tag and v.get('action',v.get('input'))=='delay-full-shutter-ui' for v in prior),'Input already intended; follow the same task, never repeat it'
 sys.path.insert(0,str(PACKAGE/'engine'));import windows_engine
 with transport_lock():
  w=windows_engine.Transport(PACKAGE);i=windows_engine.mac_patch.Installer(PACKAGE,run_batch=w.batch);one=lambda c:w.batch([c])[0].strip()
  s=read_status(one);h=json.loads(one('cat /tmp/x2d-pixel-overclock/shutter-status.json'));arm=h['arm'].split()
  assert s['ready'] and s['prepared'] and not s['busy'] and not s['error'] and h['available'] and not h['pending']
  if a.method is not None:
   assert len(arm)==9 and arm[0]=='RS4' and int(arm[3])==a.method and arm[5]=='0';delay=int(arm[6])
  else:
   assert len(arm)==8 and arm[0]==('PO3' if a.frames==6 else 'PO4') and int(arm[3])==a.frames;delay=int(arm[4])
  assert arm[1]==s['session'] and 10<=delay<=60 and arm[-2:]==['START','END']
  assert i.property('camera','image_bit_depth')==str(a.bit) and i.property('camera','exposure_status')=='0' and i.property('error','active_error')=='false'
  assert one('cat /sys/class/input/event3/device/name')=='exposure_button'
  session=arm[1];ack=int(arm[2]);record=dict(package=PACKAGE_ID,time=time.time(),case=a.tag,session=session,ack=ack,frames=a.frames,method=a.method,bit=a.bit,delay=delay,input='full-shutter-ui',action='delay-full-shutter-ui',intent_before_input=True,phase='input_unconfirmed')
  target.write_text(json.dumps(record,indent=2)+'\n',encoding='utf8')
  with journal.open('a',encoding='utf8') as f:f.write(json.dumps(record)+'\n');f.flush()
  one('for e in "1 18 1" "0 0 0" w "1 18 0" "0 0 0";do if [ "$e" = w ];then sleep .08;else sendevent /dev/input/event3 $e;fi;done')
  record['phase']='observing';target.write_text(json.dumps(record,indent=2)+'\n',encoding='utf8')
  end=time.monotonic()+8
  while True:
   s=read_status(one);assert s['session']==session and s['ack'] in (ack-1,ack)
   if s['ack']==ack and s['busy'] and s['stage']=='delay':break
   assert not s['error'] and time.monotonic()<end,'Delay not observed; no cancellation input sent'
   time.sleep(.1)
  record['delay_observed']=True;record['phase']='delay_observed';record['stage']=s['stage'];target.write_text(json.dumps(record,indent=2)+'\n',encoding='utf8')
  print(json.dumps(record),flush=True)
if __name__=='__main__':main()
