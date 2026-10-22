"""4.2.0 原厂菜单键与选择键；固定正常 UI 输入，不触发快门。"""
import argparse,json,sys,time
from pathlib import Path
from package_ui import ROOT,OUT,PACKAGE,PACKAGE_ID,transport_lock,read_status,idle_ui_allowed

def main():
    assert sys.version_info[:2]==(3,11) and Path.cwd().resolve()==ROOT
    ap=argparse.ArgumentParser();ap.add_argument('button',choices=('menu','select','escape'));a=ap.parse_args()
    # Official 4.2.0 KeyHandlerX2d initializer 0x2df8e0:
    # Menu=Qt F1, Select includes D, Escape includes Qt F2.
    event,key,name={'menu':(6,59,'gui_buttons'),'select':(5,32,'top_buttons'),'escape':(6,60,'gui_buttons')}[a.button]
    sys.path.insert(0,str(PACKAGE/'engine'));import windows_engine
    with transport_lock():
        w=windows_engine.Transport(PACKAGE);i=windows_engine.mac_patch.Installer(PACKAGE,run_batch=w.batch);one=lambda c:w.batch([c])[0].strip()
        s=read_status(one);assert idle_ui_allowed(s)
        assert i.property('camera','exposure_status')=='0' and i.property('error','active_error')=='false'
        assert one(f'cat /sys/class/input/event{event}/device/name')==name
        words=one(f'cat /sys/class/input/event{event}/device/capabilities/key').split()
        mask=sum(int(v,16)<<(64*n) for n,v in enumerate(reversed(words)));assert mask&(1<<key)
        cmd=f'for e in "1 {key} 1" "0 0 0" w "1 {key} 0" "0 0 0";do if [ "$e" = w ];then sleep .08;else sendevent /dev/input/event{event} $e;fi;done'
        try:one(cmd)
        except BaseException:
            one(f'sendevent /dev/input/event{event} 1 {key} 0; sendevent /dev/input/event{event} 0 0 0');raise
        r=dict(time=time.time(),package=PACKAGE_ID,action='factory-ui-button',button=a.button,event=event,key=key)
        with (OUT/'ui-actions.jsonl').open('a',encoding='utf8') as f:f.write(json.dumps(r)+'\n')
        print(json.dumps(r))

if __name__=='__main__':main()
