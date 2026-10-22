"""已发送唯一快门但运输中断后的25MP只读收尾；不会补拍。"""
import argparse,json,re,sys,time
from pathlib import Path
from package_ui import ROOT,OUT,PACKAGE,PACKAGE_ID,transport_lock,read_status

def main():
 assert sys.version_info[:2]==(3,11) and Path.cwd().resolve()==ROOT
 p=argparse.ArgumentParser();p.add_argument('tag');p.add_argument('--capture-case',required=True);p.add_argument('--baseline',required=True);p.add_argument('--bit',type=int,choices=(0,1),required=True);a=p.parse_args()
 assert all(re.fullmatch('[A-Za-z0-9_-]+',v) for v in (a.tag,a.capture_case,a.baseline))
 target=OUT/(a.tag+'.json');assert not target.exists()
 baseline=json.loads((OUT/(a.baseline+'.json')).read_text(encoding='utf8'))
 assert baseline['package']==PACKAGE_ID and baseline['passed'] is True
 before=baseline['metrics_after'];actions=[json.loads(s) for s in (OUT/'capture-actions.jsonl').read_text().splitlines()]
 sent=[x for x in actions if x.get('package')==PACKAGE_ID and x.get('case')==a.capture_case]
 assert len(sent)==1 and sent[0]['action']=='quarter-full-shutter-ui' and sent[0]==actions[-1]
 sys.path.insert(0,str(PACKAGE/'engine'));import windows_engine
 with transport_lock():
  w=windows_engine.Transport(PACKAGE);i=windows_engine.mac_patch.Installer(PACKAGE,run_batch=w.batch);one=lambda c:w.batch([c])[0].strip()
  s=read_status(one);metrics=json.loads(one('cat /tmp/x2d-quarter-native.metrics'))
  r=dict(package=PACKAGE_ID,time=time.time(),capture_case=a.capture_case,baseline=a.baseline,input_sent=False,photo_contents_read=False,metrics_before=before,metrics_after=metrics)
  r['exposure_status']=i.property('camera','exposure_status');r['storage_status']=i.property('storage','storage_status');r['active_error']=i.property('error','active_error');r['bit']=i.property('camera','image_bit_depth');r['mount']=one('awk \'$2=="/system"{print $4}\' /proc/mounts')
  r['passed']=metrics['raw']==before['raw']+1 and metrics['jpeg']==before['jpeg']+1 and metrics['failed']==before['failed'] and r['exposure_status']==r['storage_status']=='0' and r['active_error']=='false' and r['bit']==str(a.bit) and r['mount'].split(',')[0]=='ro' and s['ready'] and not s['busy'] and not s['prepared'] and not s['error']
 target.write_text(json.dumps(r,ensure_ascii=False,indent=2)+'\n',encoding='utf8');print(json.dumps(r));assert r['passed']

if __name__=='__main__':main()
