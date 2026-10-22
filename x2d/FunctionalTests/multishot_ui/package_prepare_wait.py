"""只读等待UI已选择模式真正准备/退出，避免把ui-ready误作后台准备完成。"""
import argparse,json,re,sys,time
from pathlib import Path
from package_ui import ROOT,OUT,PACKAGE,PACKAGE_ID,transport_lock,read_status

def main():
    p=argparse.ArgumentParser();p.add_argument('tag');p.add_argument('kind',choices=('pixel','stack','disarmed'));p.add_argument('--delay',type=int,choices=range(61));a=p.parse_args()
    assert sys.version_info[:2]==(3,11) and Path.cwd().resolve()==ROOT
    assert re.fullmatch('[A-Za-z0-9_-]+',a.tag)
    target=OUT/(a.tag+'.json');assert not target.exists()
    sys.path.insert(0,str(PACKAGE/'engine'));import windows_engine
    with transport_lock():
        w=windows_engine.Transport(PACKAGE);i=windows_engine.mac_patch.Installer(PACKAGE,run_batch=w.batch);one=lambda c:w.batch([c])[0].strip()
        end=time.monotonic()+15
        while True:
            s=read_status(one);h=json.loads(one('cat /tmp/x2d-pixel-overclock/shutter-status.json'))
            assert not s.get('busy') and not s.get('error'),'Unexpected task; no input'
            arm=h.get('arm','').split()
            matched=(not arm and not h.get('available') and not s.get('prepared')) if a.kind=='disarmed' else bool(arm and arm[0] in (('PO3','PO4') if a.kind=='pixel' else ('RS4',)) and h.get('available') and s.get('prepared'))
            if a.delay is not None:
                assert a.kind!='disarmed'
                matched=bool(matched and int(arm[4 if a.kind=='pixel' else 6])==a.delay)
            if matched and s.get('ready') and not h.get('pending'):break
            assert time.monotonic()<end,'Preparation did not settle; no input'
            time.sleep(.25)
        r=dict(package=PACKAGE_ID,kind=a.kind,input_sent=False,status={k:s.get(k) for k in ('session','ack','ready','prepared','busy','stage','error')},shutter=h)
        r['exposure_status']=i.property('camera','exposure_status');r['active_error']=i.property('error','active_error');r['mount']=one('awk \'$2=="/system"{print $4}\' /proc/mounts')
        r['passed']=r['exposure_status']=='0' and r['active_error']=='false' and r['mount'].split(',')[0]=='ro'
        target.write_text(json.dumps(r,ensure_ascii=False,indent=2)+'\n',encoding='utf8');assert r['passed']
        print('UI_MODE_READY_VERIFIED',a.tag,a.kind,flush=True)

if __name__=='__main__':main()
