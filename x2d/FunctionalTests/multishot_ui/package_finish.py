"""当前完整包已开始的堆栈任务：观察帧数后点击已确认坐标的完成按钮。"""
import argparse,json,re,sys,time
from package_ui import ROOT,OUT,PACKAGE,PACKAGE_ID,transport_lock,tap_command,read_status

def validate(s,session,ack):
    assert s.get('session')==session and s.get('ack')==ack,'Task identity changed; no touch'
    assert s.get('busy') is True and s.get('stack') is True,'Not an active stack; no touch'
    assert s.get('stage') in ('prepare','countdown','delay','capture'),'Capture stage changed; no touch'
    assert type(s.get('stack_frames')) is int and s['stack_frames']>=0,'Invalid frame count; no touch'

def main():
    assert sys.version_info[:2]==(3,11) and ROOT==__import__('pathlib').Path.cwd().resolve()
    p=argparse.ArgumentParser();p.add_argument('session');p.add_argument('ack',type=int);p.add_argument('--case',required=True);p.add_argument('--observed',type=int,default=9);p.add_argument('--timeout',type=int,default=55);p.add_argument('--x',type=int,required=True);p.add_argument('--y',type=int,required=True);a=p.parse_args()
    assert re.fullmatch('[0-9a-f]{8}',a.session) and a.ack>0
    assert re.fullmatch('[A-Za-z0-9_-]+',a.case) and 1<=a.observed<=100 and 1<=a.timeout<=120
    command=tap_command(a.x,a.y)
    sys.path.insert(0,str(PACKAGE/'engine'));import windows_engine
    with transport_lock(),(OUT/'capture-actions.jsonl').open('a',encoding='utf8') as log:
        w=windows_engine.Transport(PACKAGE)
        def one(c):return w.batch([c])[0].strip()
        def note(**values):
            v=dict(time=time.time(),package=PACKAGE_ID,case=a.case,session=a.session,ack=a.ack,**values)
            log.write(json.dumps(v)+'\n');log.flush();print(json.dumps(v),flush=True)
        end=time.monotonic()+a.timeout;last=-1
        while time.monotonic()<end:
            s=read_status(one);validate(s,a.session,a.ack)
            if s['stack_frames']!=last:last=s['stack_frames'];note(action='observed',frames=last,stage=s['stage'])
            if last>=a.observed:
                assert s['stage']=='capture','Finish not visible; no touch'
                ui=one('cat /tmp/x2d-pixel-overclock/ui.txt');sample=re.search(r'sampled_ms=(\d+)',ui)
                assert re.search(r'\bpage=1\b',ui) and sample,'Processing page absent; no touch'
                uptime=float(one('cat /proc/uptime').split()[0])*1000
                assert 0<=uptime-int(sample[1])<=2000,'Stale processing page; no touch'
                assert one('cat /sys/class/input/event8/device/name')=='himax-touchscreen'
                fresh=read_status(one);validate(fresh,a.session,a.ack)
                assert fresh['stage']=='capture'
                try:one(command)
                except BaseException:
                    note(action='finish-result-unknown',frames=fresh['stack_frames'])
                    try:one('sendevent /dev/input/event8 3 57 -1; sendevent /dev/input/event8 1 330 0; sendevent /dev/input/event8 0 0 0')
                    except BaseException:pass
                    raise
                note(action='finish-ui',frames=fresh['stack_frames'],x=a.x,y=a.y);return
            time.sleep(.1)
        raise TimeoutError('No finish touch: observation timed out')

if __name__=='__main__':main()
