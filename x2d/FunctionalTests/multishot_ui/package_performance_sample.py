"""完整包界面静止时的有界诊断；计时值不是屏幕帧率。"""
import argparse,json,re,sys,time
from pathlib import Path
from package_ui import ROOT,OUT,PACKAGE,PACKAGE_ID,transport_lock,read_status,idle_ui_allowed

def main():
    assert sys.version_info[:2]==(3,11) and Path.cwd().resolve()==ROOT
    ap=argparse.ArgumentParser();ap.add_argument('tag');a=ap.parse_args()
    assert re.fullmatch('[A-Za-z0-9_-]+',a.tag)
    target=OUT/(a.tag+'.json');assert not target.exists()
    sys.path.insert(0,str(PACKAGE/'engine'));import windows_engine
    r=dict(package=PACKAGE_ID,time=time.time(),samples=[],photo_contents_read=False,display_fps_measured=False)
    allowed=('sampled_ms','focusScans','screenScans','focusRc','focusFailures','tickMaxUs','focusUs','pixelUs','driveUs','screenUs','faceUs','samplerUs','gapMaxUs')
    with transport_lock():
        w=windows_engine.Transport(PACKAGE);i=windows_engine.mac_patch.Installer(PACKAGE,run_batch=w.batch);one=lambda c:w.batch([c])[0].strip()
        s=read_status(one);assert idle_ui_allowed(s) and not s.get('prepared')
        assert i.property('camera','exposure_status')=='0' and i.property('error','active_error')=='false'
        for _ in range(12):
            text=one('cat /tmp/x2d-pixel-overclock/ui.txt')
            fields={k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',text) if k in allowed}
            assert all(k in fields for k in ('sampled_ms','focusScans','screenScans'))
            r['samples'].append(dict(time=time.time(),**fields));time.sleep(1)
        first,last=r['samples'][3],r['samples'][-1]
        r['observation_advanced']=last['sampled_ms']>first['sampled_ms']
        r['warm_focus_scan_delta']=last['focusScans']-first['focusScans']
        r['warm_screen_scan_delta']=last['screenScans']-first['screenScans']
        r['idle_no_periodic_rescan']=r['observation_advanced'] and r['warm_focus_scan_delta']==r['warm_screen_scan_delta']==0
        r['mount']=one('awk \'$2=="/system"{print $4}\' /proc/mounts')
        r['active_error']=i.property('error','active_error')
    target.write_text(json.dumps(r,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print(json.dumps({k:v for k,v in r.items() if k!='samples'}))
    assert r['idle_no_periodic_rescan'] and r['mount'].split(',')[0]=='ro' and r['active_error']=='false'

if __name__=='__main__':main()
