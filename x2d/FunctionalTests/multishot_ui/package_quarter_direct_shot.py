"""完整包25MP：由UI选入后只按一次实体快门，核对原厂存储转换计数；不读照片。"""
import argparse,json,sys,time,re
from pathlib import Path
from package_ui import ROOT,OUT,PACKAGE,PACKAGE_ID,transport_lock

def main():
 assert sys.version_info[:2]==(3,11) and Path.cwd().resolve()==ROOT
 ap=argparse.ArgumentParser();ap.add_argument('tag');ap.add_argument('--bit',type=int,choices=(0,1),required=True);a=ap.parse_args();tag=a.tag;assert re.fullmatch('[A-Za-z0-9_-]+',tag);target=OUT/(tag+'.json');assert not target.exists()
 sys.path.insert(0,str(PACKAGE/'engine'));import windows_engine
 r={'package':PACKAGE_ID,'time':time.time(),'photo_contents_read':False,'observations':[]}
 with transport_lock():
  w=windows_engine.Transport(PACKAGE);i=windows_engine.mac_patch.Installer(PACKAGE,run_batch=w.batch);one=lambda c:w.batch([c])[0].strip()
  def metrics():return json.loads(one('if test -f /tmp/x2d-quarter-native.metrics;then cat /tmp/x2d-quarter-native.metrics;else echo \'{"raw":0,"jpeg":0,"failed":0}\';fi'))
  assert 'NATIVE_UI_READY_CURRENT_REQUEST' in one('/system/bin/x2d-native-flow ui-ready')
  assert one('cat /tmp/x2d-quarter-native.status')=='QUARTER_NATIVE_READY'
  assert one('cat /tmp/x2d-pixel-overclock/quarter.txt')=='QN1 25 END'
  h=json.loads(one('cat /tmp/x2d-pixel-overclock/shutter-status.json'));assert h['ready'] and not h['arm'] and not h['pending']
  assert one('cat /tmp/x2d-quarter-upstream.status')=='READY_PRELOAD'
  def upstream():return json.loads(one('if test -f /tmp/x2d-quarter-upstream.metrics;then cat /tmp/x2d-quarter-upstream.metrics;else echo \'{"direct":0,"fallback":0,"failed":0,"blocked":0}\';fi'))
  up_before=upstream();r['upstream_before']=up_before
  r['image_bit_depth']=i.property('camera','image_bit_depth');assert r['image_bit_depth']==str(a.bit)
  r['tv_current']=i.property('camera','tv_current')
  r['timing_includes_exposure']=True
  assert i.property('camera','drive_mode')=='0' and i.property('camera','image_format')=='1'
  assert i.property('camera','exposure_status')=='0' and i.property('storage','storage_status')=='0' and i.property('error','active_error')=='false'
  assert one('cat /sys/class/input/event3/device/name')=='exposure_button'
  before=metrics();r['metrics_before']=before
  started=time.monotonic();r['shutter_request_wall_time']=time.time()
  one('for e in "1 18 1" "0 0 0" w "1 18 0" "0 0 0";do if [ "$e" = w ];then sleep .08;else sendevent /dev/input/event3 $e;fi;done')
  r['full_shutter_sent']=True;print('FULL_SHUTTER_UI_INPUT_SENT',flush=True)
  with (OUT/'capture-actions.jsonl').open('a',encoding='utf8') as f:f.write(json.dumps(dict(package=PACKAGE_ID,case=tag,time=time.time(),action='quarter-full-shutter-ui'))+'\n')
  end=time.monotonic()+120;previous=None
  while True:
   m=metrics();exposure=i.property('camera','exposure_status');storage=i.property('storage','storage_status');error=i.property('error','active_error');state=(m,exposure,storage,error)
   if state!=previous:
    sample={'time':time.time(),'metrics':m,'exposure':exposure,'storage':storage,'active_error':error};r['observations'].append(sample);print(json.dumps(sample),flush=True);previous=state
   if error!='false' or m['failed']!=before['failed']:break
   if m['raw']==before['raw']+1 and m['jpeg']==before['jpeg']+1 and exposure==storage=='0':break
   if time.monotonic()>=end:break
   time.sleep(.5)
  r['shutter_to_idle_observed_ms']=round((time.monotonic()-started)*1000,1)
  r['timing_limit']='主机USB轮询得到的结束观测上界，不是硬件完成时刻'
  after=upstream();r['upstream_after']=after
  r['direct_hit']=after['direct']==up_before['direct']+1 and after['failed']==up_before['failed'] and not after['blocked']
  r['mount']=one('awk \'$2=="/system"{print $4}\' /proc/mounts');r['metrics_after']=m;r['active_error']=error;r['exposure_status']=exposure;r['storage_status']=storage
  r['passed']=r['direct_hit'] and m['raw']==before['raw']+1 and m['jpeg']==before['jpeg']+1 and m['failed']==before['failed'] and exposure==storage=='0' and error=='false' and r['mount'].split(',')[0]=='ro'
  target.write_text(json.dumps(r,indent=2)+'\n',encoding='utf8');assert r['passed'],'25MP output not accepted; no second shutter'
  print('QUARTER_RAW_JPEG_COUNTERS_IDLE_VERIFIED',flush=True)
if __name__=='__main__':main()
