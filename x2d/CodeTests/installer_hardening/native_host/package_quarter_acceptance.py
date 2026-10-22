"""完整包25MP：由UI选入后只按一次实体快门，核对原厂存储转换计数；不读照片。"""
import argparse,json,sys,time,re,hashlib
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[3]/'FunctionalTests/multishot_ui'))
from package_ui import ROOT,OUT,PACKAGE,PACKAGE_ID,transport_lock

def main():
 assert sys.version_info[:2]==(3,11) and Path.cwd().resolve()==ROOT
 ap=argparse.ArgumentParser();ap.add_argument('tag');ap.add_argument('--bit',type=int,choices=(0,1),required=True);a=ap.parse_args();tag=a.tag;assert re.fullmatch('[A-Za-z0-9_-]+',tag);target=OUT/(tag+'.json');assert not target.exists()
 journal=OUT/'capture-actions.jsonl'
 prior=[json.loads(line) for line in journal.read_text(encoding='utf8').splitlines()] if journal.exists() else []
 assert not any(v.get('package')==PACKAGE_ID and v.get('case')==tag and v.get('action')=='quarter-full-shutter-ui' for v in prior),'Input already intended; follow the same capture, never repeat it'
 sys.path.insert(0,str(PACKAGE/'engine'));import windows_engine
 r={'package':PACKAGE_ID,'package_exe_sha256':hashlib.sha256((PACKAGE/'HasselbladTuningPatchInstaller.exe').read_bytes()).hexdigest(),'time':time.time(),'photo_contents_read':False,'observations':[]}
 with transport_lock():
  w=windows_engine.Transport(PACKAGE);i=windows_engine.mac_patch.Installer(PACKAGE,run_batch=w.batch);one=lambda c:w.batch([c])[0].strip()
  def metrics():return json.loads(one('if test -f /tmp/x2d-quarter-native.metrics;then cat /tmp/x2d-quarter-native.metrics;else echo \'{"raw":0,"jpeg":0,"failed":0}\';fi'))
  assert 'NATIVE_UI_READY_CURRENT_REQUEST' in one('/system/bin/x2d-native-flow ui-ready')
  assert one('cat /tmp/x2d-quarter-native.status')=='QUARTER_NATIVE_READY'
  assert one('cat /tmp/x2d-pixel-overclock/quarter.txt')=='QN1 25 END'
  h=json.loads(one('cat /tmp/x2d-pixel-overclock/shutter-status.json'));assert h['ready'] and not h['arm'] and not h['pending']
  r['image_bit_depth']=i.property('camera','image_bit_depth');assert r['image_bit_depth']==str(a.bit)
  assert i.property('camera','drive_mode')=='0' and i.property('camera','image_format')=='1'
  assert i.property('camera','exposure_status')=='0' and i.property('storage','storage_status')=='0' and i.property('error','active_error')=='false'
  assert one('cat /sys/class/input/event3/device/name')=='exposure_button'
  before=metrics();r['metrics_before']=before
  r['pre_shutter']={key:i.property('camera',key) for key in ('tv','focus_mode','image_bit_depth','exposure_status','drive_mode')}
  # Preserve the counter baseline and intent before input, even if USB loses
  # the acknowledgement. A retry with this case must not take another photo.
  r.update(passed=False,phase='input_unconfirmed',intent_before_input=True)
  target.write_text(json.dumps(r,indent=2)+'\n',encoding='utf8')
  with journal.open('a',encoding='utf8') as f:
   f.write(json.dumps(dict(package=PACKAGE_ID,case=tag,time=time.time(),action='quarter-full-shutter-ui',intent_before_input=True))+'\n');f.flush()
  one('for e in "1 18 1" "0 0 0" w "1 18 0" "0 0 0";do if [ "$e" = w ];then sleep .08;else sendevent /dev/input/event3 $e;fi;done')
  r['full_shutter_sent']=True;r['phase']='observing';target.write_text(json.dumps(r,indent=2)+'\n',encoding='utf8');print('FULL_SHUTTER_UI_INPUT_SENT',flush=True)
  end=time.monotonic()+120;previous=None
  while True:
   m=metrics();exposure=i.property('camera','exposure_status');storage=i.property('storage','storage_status');error=i.property('error','active_error');state=(m,exposure,storage,error)
   if state!=previous:
    sample={'time':time.time(),'metrics':m,'exposure':exposure,'storage':storage,'active_error':error};r['observations'].append(sample);target.write_text(json.dumps(r,indent=2)+'\n',encoding='utf8');print(json.dumps(sample),flush=True);previous=state
   if error!='false' or m['failed']!=before['failed'] or (exposure.isdecimal() and int(exposure)&128):
    r['factory_exposure_failed']=bool(exposure.isdecimal() and int(exposure)&128);break
   if m['raw']==before['raw']+1 and m['jpeg']==before['jpeg']+1 and exposure==storage=='0':break
   if time.monotonic()>=end:break
   time.sleep(.5)
  r['mount']=one('awk \'$2=="/system"{print $4}\' /proc/mounts');r['metrics_after']=m;r['active_error']=error;r['exposure_status']=exposure;r['storage_status']=storage
  r['passed']=m['raw']==before['raw']+1 and m['jpeg']==before['jpeg']+1 and m['failed']==before['failed'] and exposure==storage=='0' and error=='false' and r['mount'].split(',')[0]=='ro'
  r['post_shutter']={key:i.property('camera',key) for key in ('tv','focus_mode','image_bit_depth','exposure_status','drive_mode')}
  r['phase']='accepted' if r['passed'] else 'not_accepted'
  target.write_text(json.dumps(r,indent=2)+'\n',encoding='utf8');assert r['passed'],'25MP output not accepted; no second shutter'
  print('QUARTER_RAW_JPEG_COUNTERS_IDLE_VERIFIED',flush=True)
if __name__=='__main__':main()
