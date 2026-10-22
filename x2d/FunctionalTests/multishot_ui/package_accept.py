"""对已截图确认的弹窗发送原厂F4应用键，固定4.2.0映射；不合成快门。"""
import argparse,json,re,sys,time
from pathlib import Path
from package_ui import ROOT,OUT,PACKAGE,PACKAGE_ID,transport_lock,read_status,idle_ui_allowed

def main():
    assert sys.version_info[:2]==(3,11) and Path.cwd().resolve()==ROOT
    ap=argparse.ArgumentParser();ap.add_argument('case');a=ap.parse_args();assert re.fullmatch('[A-Za-z0-9_-]+',a.case)
    sys.path.insert(0,str(PACKAGE/'engine'));import windows_engine
    with transport_lock():
        w=windows_engine.Transport(PACKAGE);i=windows_engine.mac_patch.Installer(PACKAGE,run_batch=w.batch);one=lambda c:w.batch([c])[0].strip()
        s=read_status(one);assert idle_ui_allowed(s)
        assert i.property('camera','exposure_status')=='0' and i.property('error','active_error')=='false'
        assert one('cat /sys/class/input/event6/device/name')=='gui_buttons'
        assert int(one('cat /sys/class/input/event6/device/capabilities/key'),16)&(1<<62)
        command='for e in "1 62 1" "0 0 0" w "1 62 0" "0 0 0";do if [ "$e" = w ];then sleep .08;else sendevent /dev/input/event6 $e;fi;done'
        try:one(command)
        except BaseException:
            one('sendevent /dev/input/event6 1 62 0; sendevent /dev/input/event6 0 0 0');raise
        r=dict(time=time.time(),package=PACKAGE_ID,action='physical-popup-accept',case=a.case,event=6,key=62,shutter_input=False)
        with (OUT/'ui-actions.jsonl').open('a',encoding='utf8') as f:f.write(json.dumps(r)+'\n')
        print(json.dumps(r))

if __name__=='__main__':main()
