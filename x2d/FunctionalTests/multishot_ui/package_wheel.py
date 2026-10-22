"""经4.2.0映射、既有输入清单与实时能力核对的前／后拨轮UI导航。"""
import argparse,json,sys,time
from pathlib import Path
from package_ui import ROOT,OUT,PACKAGE,PACKAGE_ID,transport_lock,read_status,idle_ui_allowed

def main():
    assert sys.version_info[:2]==(3,11) and Path.cwd().resolve()==ROOT
    ap=argparse.ArgumentParser();ap.add_argument('direction',choices=('up','down'));ap.add_argument('--wheel',choices=('rear','front'),default='rear');ap.add_argument('--steps',type=int,choices=range(1,33),default=1);a=ap.parse_args();event=0 if a.wheel=='rear' else 1;name='back_tw' if a.wheel=='rear' else 'front_tw';key=((103 if a.direction=='up' else 108) if a.wheel=='rear' else (105 if a.direction=='up' else 106))
    sys.path.insert(0,str(PACKAGE/'engine'));import windows_engine
    with transport_lock():
        w=windows_engine.Transport(PACKAGE);i=windows_engine.mac_patch.Installer(PACKAGE,run_batch=w.batch);one=lambda c:w.batch([c])[0].strip()
        s=read_status(one);assert idle_ui_allowed(s)
        assert i.property('camera','exposure_status')=='0' and i.property('error','active_error')=='false'
        assert one(f'cat /sys/class/input/event{event}/device/name')==name
        words=one(f'cat /sys/class/input/event{event}/device/capabilities/key').split()
        mask=sum(int(v,16)<<(64*n) for n,v in enumerate(reversed(words)));assert mask&(1<<key)
        cmd=f'for n in $(seq 1 {a.steps});do for e in "1 {key} 1" "0 0 0" w "1 {key} 0" "0 0 0";do if [ "$e" = w ];then sleep .08;else sendevent /dev/input/event{event} $e;fi;done;done'
        try:one(cmd)
        except BaseException:
            one(f'sendevent /dev/input/event{event} 1 {key} 0; sendevent /dev/input/event{event} 0 0 0');raise
        r=dict(time=time.time(),package=PACKAGE_ID,action=a.wheel+'-wheel-event',event=event,key=key,direction=a.direction,steps=a.steps)
        with (OUT/'ui-actions.jsonl').open('a',encoding='utf8') as f:f.write(json.dumps(r)+'\n')
        print(json.dumps(r))

if __name__=='__main__':main()
