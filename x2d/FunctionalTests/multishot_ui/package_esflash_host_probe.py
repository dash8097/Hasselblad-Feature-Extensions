"""只读核对包内引闪服务的身份和自身映射；不调用服务或输出闪光。"""
from pathlib import Path
import argparse,json,sys,time
from package_ui import ROOT,OUT,PACKAGE,PACKAGE_ID,transport_lock
assert sys.version_info[:2]==(3,11) and Path.cwd().resolve()==ROOT
p=argparse.ArgumentParser();p.add_argument('tag');a=p.parse_args()
assert a.tag.replace('-','').isalnum();target=OUT/(a.tag+'.json');assert not target.exists()
sys.path.insert(0,str(PACKAGE/'engine'));import windows_engine
r={'package':PACKAGE_ID,'time':time.time()}
with transport_lock():
 w=windows_engine.Transport(PACKAGE);one=lambda c:w.batch([c])[0].strip()
 # The normal parent executes the validated descriptor. Linux comm can then
 # be the descriptor number, so bind to the actual executable instead of comm.
 found=one('for p in /proc/[0-9]*;do test "$(readlink "$p/exe")" = /system/bin/x2d-esflash && echo "${p##*/}";done;true').split()
 assert 1<=len(found)<=2 and all(p.isdigit() for p in found),'Unexpected installed component count'
 parents={p:one('grep ^PPid: /proc/'+p+'/status').split()[-1] for p in found}
 # Resident output protection deliberately forks one guardian from the worker.
 owners=[p for p in found if parents[p] not in found];assert len(owners)==1
 pid=owners[0];r['pid']=int(pid);r['identity_method']='exact-executable-and-parent-relationship'
 guardians=[p for p in found if p!=pid]
 assert not guardians or parents[guardians[0]]==pid
 r['guardian_pid']=int(guardians[0]) if guardians else None
 for key,c in (
  ('exe','readlink /proc/'+pid+'/exe'),
  ('argv','tr \'\\000\' \' \' </proc/'+pid+'/cmdline'),
  ('uid','grep ^Uid: /proc/'+pid+'/status'),
  ('stdout','readlink /proc/'+pid+'/fd/1'),
  ('stderr','readlink /proc/'+pid+'/fd/2'),
  ('library_maps','grep -E \'librcam.so|libx2d_esflash_hotshoe.so\' /proc/'+pid+'/maps || true'),
  ('rcam_sha','sha256sum /system/lib64/librcam.so'),
  ('helper_sha','sha256sum /system/lib64/libx2d_esflash_hotshoe.so'),
  ('mount','awk \'$2=="/system"{print $4}\' /proc/mounts')):
  r[key]=one(c)
 service=one('pidof camera-service');assert service.isdigit();r['camera_service_pid']=int(service)
 r['camera_service_patch_maps']=one('grep -E \'libx2d_|librcam.so\' /proc/'+service+'/maps || true')
 r['render_route_stat']=one('stat -c \'%F %u %h\' /tmp/x2d-pixel-render-route.status 2>/dev/null || echo ABSENT')
 r['render_route']=one('cat /tmp/x2d-pixel-render-route.status 2>/dev/null || true')
 r['temporary_root']=one('ls -ldZ /tmp')
 r['render_route_temporary']=one('ls -ldZ /tmp/x2d-pixel-render-route.tmp 2>/dev/null || echo ABSENT')
 r['service_context']=one('cat /proc/'+service+'/attr/current')
 r['flash_context']=one('cat /proc/'+pid+'/attr/current')
 r['known_path_denials']=one("dmesg | grep 'avc:.*denied' | grep -E 'x2d-pixel-render-route|libx2d_esflash_hotshoe|perf_event|/dev/mem' | tail -n 12 || true")
target.write_text(json.dumps(r,indent=2)+'\n',encoding='utf8');print(json.dumps(r))
