"""只读取当前已结束堆栈任务的数字就绪诊断；不读取成片或保留完整日志。"""
from pathlib import Path
import argparse,base64,json,re,sys,time
from package_ui import ROOT,OUT,PACKAGE,PACKAGE_ID,transport_lock,read_status
assert sys.version_info[:2]==(3,11) and Path.cwd().resolve()==ROOT
p=argparse.ArgumentParser();p.add_argument('tag');p.add_argument('session');p.add_argument('ack',type=int);a=p.parse_args()
assert re.fullmatch('[A-Za-z0-9_-]+',a.tag) and re.fullmatch('[a-f0-9]{8}',a.session) and a.ack>0
target=OUT/(a.tag+'.json');assert not target.exists()
sys.path.insert(0,str(PACKAGE/'engine'));import windows_engine
r={'package':PACKAGE_ID,'time':time.time(),'session':a.session,'ack':a.ack,'photo_contents_read':False,'admission_states':[]}
with transport_lock():
 w=windows_engine.Transport(PACKAGE);one=lambda c:w.batch([c])[0].strip();s=read_status(one)
 assert s.get('session')==a.session and s.get('ack')==a.ack and s.get('busy') is False and not s.get('prepared')
 def control(path):
  data=one('/system/xbin/busybox base64 '+path);assert len(data)<262144
  return base64.b64decode(''.join(data.split()),validate=True).decode('utf8',errors='replace')
 receipt=control('/tmp/x2d-pixel-overclock/last-task.txt')
 jobs=set(re.findall(r'^job_id=(native-album-ui-'+a.session+r'-[1-9][0-9]{0,4})$',receipt,re.M));assert len(jobs)==1
 r['job_id']=next(iter(jobs));text=control('/mnt/media_rw/ssd/multishot/'+r['job_id']+'/run.log')
 pattern=re.compile(r'^STACK_ADMISSION_WAIT exposure_status=([0-9]+) active_error=(true|false) storage_status=([0-9]+)$')
 for line in text.splitlines():
  m=pattern.fullmatch(line)
  if m:r['admission_states'].append({'exposure':int(m[1]),'active_error':m[2]=='true','storage':int(m[3])})
 r['completed']=s.get('stage')=='complete' and not s.get('error')
 r['observed_storage_128']=any(v['storage']==128 for v in r['admission_states'])
 assert r['admission_states'],'Current task has no new admission diagnostics'
target.write_text(json.dumps(r,indent=2)+'\n',encoding='utf8');print(json.dumps(r))
