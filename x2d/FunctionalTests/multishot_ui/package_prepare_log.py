"""读取已知准备失败任务的控制日志；仅保留失败检查点、退出码与系统组件名。"""
from pathlib import Path
import argparse,base64,json,re,sys
from package_ui import ROOT,OUT,PACKAGE,PACKAGE_ID,transport_lock,read_status
assert sys.version_info[:2]==(3,11) and Path.cwd().resolve()==ROOT
p=argparse.ArgumentParser();p.add_argument('tag');p.add_argument('session');p.add_argument('job',type=int);a=p.parse_args()
assert re.fullmatch('[A-Za-z0-9_-]+',a.tag) and re.fullmatch('[a-f0-9]{8}',a.session) and 0<a.job<10000
t=OUT/(a.tag+'.json');assert not t.exists();sys.path.insert(0,str(PACKAGE/'engine'));import windows_engine
r={'package':PACKAGE_ID,'job_id':'native-album-ui-'+a.session+'-'+str(a.job),'diagnostics':[],'photo_contents_read':False}
with transport_lock():
 w=windows_engine.Transport(PACKAGE);one=lambda c:w.batch([c])[0].strip();s=read_status(one)
 assert s.get('session')==a.session and s.get('busy') is False and s.get('prepared') is False
 path='/mnt/media_rw/ssd/multishot/'+r['job_id']+'/run.log'
 r['bytes']=int(one('stat -c %s '+path));assert r['bytes']<=32768
 encoded=one('/system/xbin/busybox base64 '+path)
 text=base64.b64decode(''.join(encoded.split()),validate=True).decode('utf8',errors='replace')
 for line in text.splitlines():
  fields=dict(re.findall(r'\b(step|primary_rc|restore_rc|errno|rc)=([^\s]+)',line))
  if not fields:continue
  if 'step' in fields:
   name,sep,component=fields['step'].partition(':');assert re.fullmatch('[A-Za-z0-9_-]+',name)
   fields['step']=name
   if sep and ((component.startswith('/system/') and re.fullmatch('/system/[A-Za-z0-9_/.-]+',component)) or component=='/tmp/x2d-pixel-render-route.status'):fields['component']=component
  r['diagnostics'].append(fields)
t.write_text(json.dumps(r,indent=2)+'\n',encoding='utf8');print(json.dumps(r))
