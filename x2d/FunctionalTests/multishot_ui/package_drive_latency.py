"""实际UI模式切换的低干扰计时；采样期间不发D-Bus属性或ui-ready检查。"""
import argparse,json,re,sys,time
from pathlib import Path
from package_ui import ROOT,OUT,PACKAGE,PACKAGE_ID,transport_lock,read_status,idle_ui_allowed,tap_command
def main():
 assert sys.version_info[:2]==(3,11) and Path.cwd().resolve()==ROOT
 parser=argparse.ArgumentParser();parser.add_argument('tag');a=parser.parse_args();assert re.fullmatch('[A-Za-z0-9_-]+',a.tag)
 target=OUT/(a.tag+'.json');assert not target.exists()
 sys.path.insert(0,str(PACKAGE/'engine'));import windows_engine
 r=dict(package=PACKAGE_ID,time=time.time(),display_fps_measured=False,dbus_probes_during_transitions=0,transitions=[])
 with transport_lock():
  w=windows_engine.Transport(PACKAGE);i=windows_engine.mac_patch.Installer(PACKAGE,run_batch=w.batch);one=lambda c:w.batch([c])[0].strip()
  initial=read_status(one)
  assert idle_ui_allowed(initial) and not initial.get('prepared') and i.property('camera','exposure_status')=='0' and i.property('camera','drive_mode')=='0' and i.property('error','active_error')=='false'
  assert one('cat /tmp/x2d-pixel-overclock/quarter.txt')=='DISARMED'
  for event,name in ((8,'himax-touchscreen'),(6,'gui_buttons')):assert one(f'cat /sys/class/input/event{event}/device/name')==name
  assert int(one('cat /sys/class/input/event6/device/capabilities/key'),16)&(1<<62)
  def action(kind,command,**fields):
   one(command)
   with (OUT/'ui-actions.jsonl').open('a',encoding='utf8') as f:f.write(json.dumps(dict(time=time.time(),package=PACKAGE_ID,case=a.tag,action=kind,**fields))+'\n')
  def tap(x,y):action('tap',tap_command(x,y),values=[x,y]);time.sleep(.8)
  key='for e in "1 62 1" "0 0 0" w "1 62 0" "0 0 0";do if [ "$e" = w ];then sleep .08;else sendevent /dev/input/event6 $e;fi;done'
  time.sleep(1)
  for cycle in range(3):
   for mode,x in ((1,365),(0,170)):
    tap(305,710)
    context={k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',one('cat /tmp/x2d-pixel-overclock/ui.txt'))}
    assert context.get('popup')==1,'Drive popup did not open; stop before selecting a coordinate'
    tap(x,195);action('physical-popup-accept',key,button='apply',event=6,key=62)
    item=dict(cycle=cycle+1,requested_mode=mode,samples=[]);seen=set()
    for _ in range(12):
     fields={k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',one('cat /tmp/x2d-pixel-overclock/ui.txt')) if k in ('sampled_ms','popup','tickMaxUs','driveUs','pixelUs','playbackAboutUs','animationUs','focusUs','screenUs','samplerUs','discoveryUs','gapMaxUs','driveViewUs','driveProxyUs','driveIntentUs','driveActionUs','guardFlashUs','rootSelectUs')}
     assert 'sampled_ms' in fields
     if fields['sampled_ms'] not in seen:item['samples'].append(fields);seen.add(fields['sampled_ms'])
     time.sleep(.1)
    assert len(item['samples'])>=2 and item['samples'][-1]['popup']==0
    # Read back only after timing samples: a closed popup alone does not prove
    # that the requested drive mode was actually applied.
    item['applied_mode']=i.property('camera','drive_mode')
    r['transitions'].append(item)
    if item['applied_mode']!=str(mode):
     r['failure']='Popup closed without applying requested mode';target.write_text(json.dumps(r,indent=2)+'\n',encoding='utf8');raise AssertionError(r['failure'])
    print(json.dumps(dict(cycle=cycle+1,mode=mode,max_tick_us=max(s.get('tickMaxUs',0) for s in item['samples']),max_drive_us=max(s.get('driveUs',0) for s in item['samples']))),flush=True)
  r['drive_mode']=i.property('camera','drive_mode');r['exposure_status']=i.property('camera','exposure_status');r['active_error']=i.property('error','active_error');r['mount']=one('awk \'$2=="/system"{print $4}\' /proc/mounts')
  assert r['drive_mode']==r['exposure_status']=='0' and r['active_error']=='false' and r['mount'].split(',')[0]=='ro'
 target.write_text(json.dumps(r,indent=2)+'\n',encoding='utf8')
 print('LOW_INTRUSION_UI_OBSERVATION_COMPLETE_NO_FPS_CLAIM')
if __name__=='__main__':main()
