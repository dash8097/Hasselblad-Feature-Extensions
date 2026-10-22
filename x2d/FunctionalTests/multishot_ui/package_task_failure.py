"""仅取调度器已脱敏的固定失败归档及活动状态，不读取成片。"""
from pathlib import Path
import argparse,base64,json,re,sys,time
from package_ui import ROOT,OUT,PACKAGE,PACKAGE_ID,transport_lock,read_status
assert sys.version_info[:2]==(3,11) and Path.cwd().resolve()==ROOT
p=argparse.ArgumentParser();p.add_argument('tag');a=p.parse_args();assert re.fullmatch('[A-Za-z0-9_-]+',a.tag)
target=OUT/(a.tag+'.json');assert not target.exists()
sys.path.insert(0,str(PACKAGE/'engine'));import windows_engine
r={'package':PACKAGE_ID,'time':time.time(),'photo_contents_read':False}
with transport_lock():
 w=windows_engine.Transport(PACKAGE);one=lambda c:w.batch([c])[0].strip();i=windows_engine.mac_patch.Installer(PACKAGE,run_batch=w.batch)
 s=read_status(one);r['status']={k:s[k] for k in ('stage','error','busy','ready','prepared','session','ack') if k in s}
 data=one('/system/xbin/busybox base64 /tmp/x2d-pixel-overclock/last-failed-task.txt');assert len(data)<16384
 lines=base64.b64decode(''.join(data.split()),validate=True).decode('utf8',errors='replace').splitlines()
 r['task_diagnostics']=[v for v in lines if not any(x in v.lower() for x in ('/','\\','serial','uid','.jpg','.3fr','.heic'))]
 r['task_lock']=one('if test -d /tmp/x2d-multishot-active;then echo PRESENT;else echo ABSENT;fi')
 r['native_flow_pids']=one('pidof x2d-native-flow || true')
 r['exposure_status']=i.property('camera','exposure_status');r['storage_status']=i.property('storage','storage_status');r['active_error']=i.property('error','active_error')
 r['mount']=one('awk \'$2=="/system"{print $4}\' /proc/mounts')
target.write_text(json.dumps(r,ensure_ascii=False,indent=2)+'\n',encoding='utf8');print(json.dumps(r,ensure_ascii=False))
