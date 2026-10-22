"""UI改参数后的只读时序验收；不连接闪光服务、不设置属性或触发拍摄。"""
import argparse,json,re,sys,time
from pathlib import Path
from package_ui import ROOT,OUT,PACKAGE,PACKAGE_ID,transport_lock,read_status,idle_ui_allowed

def main():
 assert sys.version_info[:2]==(3,11) and Path.cwd().resolve()==ROOT
 p=argparse.ArgumentParser();p.add_argument('tag');p.add_argument('--profile',type=int,choices=range(5),required=True);p.add_argument('--bit',type=int,choices=(0,1),required=True);p.add_argument('--tv',type=int,required=True);a=p.parse_args()
 assert re.fullmatch('[A-Za-z0-9_-]+',a.tag)
 target=OUT/(a.tag+'.json');assert not target.exists()
 sys.path.insert(0,str(PACKAGE/'engine'));import windows_engine
 record=dict(package=PACKAGE_ID,time=time.time(),expected=dict(profile=a.profile,bit=a.bit,tv=a.tv),observations=[],photo_contents_read=False,lamp_mode_commands=0,optical_sync_measured=False)
 with transport_lock():
  w=windows_engine.Transport(PACKAGE);i=windows_engine.mac_patch.Installer(PACKAGE,run_batch=w.batch);one=lambda c:w.batch([c])[0].strip()
  s=read_status(one);assert idle_ui_allowed(s) and not s.get('prepared')
  assert i.property('camera','exposure_status')=='0' and i.property('error','active_error')=='false'
  assert i.property('camera','image_bit_depth')==str(a.bit) and i.property('camera','tv_current')==str(a.tv)
  first=None;passed=False;end=time.monotonic()+10
  while time.monotonic()<end:
   fields={k:int(v) for k,v in re.findall(r'(\w+)=(-?\d+)',one('cat /tmp/x2d-pixel-overclock/ui.txt')) if k=='sampled_ms' or k.startswith('flash')}
   assert 'sampled_ms' in fields
   if first is None:first=fields['sampled_ms']
   record['observations'].append(dict(time=time.time(),**fields))
   expected={'flashReady':1,'flashPending':0,'flashProfile':a.profile,'flashRequested':int(a.profile!=0),'flashSampling':int(a.profile!=0),'flashFault':0,'flashExposureValid':1,'flashElectronic':int(a.profile!=0),'flashDepth':a.bit,'flashTv':a.tv}
   if a.profile:expected['flashDelayUs']={1:168000,2:160000,3:370000,4:370000}[a.profile]
   if fields['sampled_ms']>first and all(fields.get(k)==v for k,v in expected.items()):passed=True;break
   time.sleep(.25)
  record['mount']=one('awk \'$2=="/system"{print $4}\' /proc/mounts')
  record['active_error']=i.property('error','active_error')
  record['passed']=passed and record['mount'].split(',')[0]=='ro' and record['active_error']=='false'
 target.write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
 print(json.dumps({k:v for k,v in record.items() if k!='observations'}))
 assert record['passed'],'actual GUI timing receipt differs; no configuration command sent by this test'

if __name__=='__main__':main()
