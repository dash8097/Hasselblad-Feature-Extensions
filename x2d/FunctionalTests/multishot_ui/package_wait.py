"""等同一实机任务收尾后再允许下一组 UI 操作；不读取照片。"""
import argparse,json,re,sys,time
from pathlib import Path
from package_ui import ROOT,OUT,PACKAGE,PACKAGE_ID,transport_lock,read_status

def main():
    p=argparse.ArgumentParser();p.add_argument('tag');p.add_argument('session');p.add_argument('ack',type=int);p.add_argument('--frames',type=int,default=10);p.add_argument('--minimum-frames',type=int);p.add_argument('--natural',action='store_true');p.add_argument('--require-flash',action='store_true');p.add_argument('--timeout',type=int,default=90);a=p.parse_args()
    assert sys.version_info[:2]==(3,11) and Path.cwd().resolve()==ROOT
    assert re.fullmatch('[A-Za-z0-9_-]+',a.tag) and re.fullmatch('[0-9a-f]{8}',a.session) and a.ack>0 and 1<=a.timeout<=180
    assert a.minimum_frames is None or (not a.natural and 2<=a.minimum_frames<=65535)
    target=OUT/(a.tag+'.json');checkpoint=OUT/(a.tag+'-observing.json');assert not target.exists() and not checkpoint.exists()
    sys.path.insert(0,str(PACKAGE/'engine'));import windows_engine
    with transport_lock():
        w=windows_engine.Transport(PACKAGE);i=windows_engine.mac_patch.Installer(PACKAGE,run_batch=w.batch)
        def one(c):return w.batch([c])[0].strip()
        end=time.monotonic()+a.timeout;previous=None;trace=[];flash_samples=[];flash_failure=None
        while True:
            s=read_status(one);assert s.get('session')==a.session and s.get('ack')==a.ack,'Task identity changed'
            if a.require_flash:
                fields={k:int(v) for k,v in re.findall(r'(\w+)=(-?\d+)',one('cat /tmp/x2d-pixel-overclock/ui.txt')) if k=='sampled_ms' or k.startswith('flash')}
                flash_samples.append(dict(time=time.time(),stage=s.get('stage'),frames=s.get('stack_frames'),**fields))
                # Keep following the same task through natural cleanup even if
                # flash fails; never retry a pulse or leave an active capture.
                if flash_failure is None and any(fields.get(k)!=0 for k in ('flashFault','flashError','flashErrno')):
                    flash_failure=flash_samples[-1]
            state=(s.get('stage'),s.get('stack_frames'),s.get('busy'))
            if state!=previous:
                sample=dict(time=time.time(),stage=state[0],frames=state[1],busy=state[2],remaining=s.get('stack_remaining'));trace.append(sample);print(json.dumps(sample),flush=True);previous=state
            checkpoint.write_text(json.dumps(dict(package=PACKAGE_ID,session=a.session,ack=a.ack,observations=trace,flash_samples=flash_samples,first_flash_failure=flash_failure),ensure_ascii=False,indent=2)+'\n',encoding='utf8')
            if s.get('busy') is False:
                # Publication completion may precede the normal asynchronous
                # flash configuration receipt. Observe only this same idle
                # task for at most 10 seconds; never reconfigure or retry.
                if a.require_flash and flash_failure is None and (fields.get('flashPending')==1 or fields.get('sampled_ms',0)<=flash_samples[0].get('sampled_ms',0)):
                    settle_end=time.monotonic()+10
                    while (fields.get('flashPending')==1 or fields.get('sampled_ms',0)<=flash_samples[0].get('sampled_ms',0)) and time.monotonic()<settle_end:
                        time.sleep(.5)
                        settled=read_status(one)
                        assert settled.get('session')==a.session and settled.get('ack')==a.ack and settled.get('busy') is False,'Idle task identity changed'
                        fields={k:int(v) for k,v in re.findall(r'(\w+)=(-?\d+)',one('cat /tmp/x2d-pixel-overclock/ui.txt')) if k=='sampled_ms' or k.startswith('flash')}
                        flash_samples.append(dict(time=time.time(),stage=settled.get('stage'),frames=settled.get('stack_frames'),settling=True,**fields))
                        if any(fields.get(k)!=0 for k in ('flashFault','flashError','flashErrno')):
                            flash_failure=flash_samples[-1];break
                break
            assert time.monotonic()<end,'Task still busy; do not change UI'
            time.sleep(1)
        r=dict(package=PACKAGE_ID,time=time.time(),status={k:s[k] for k in ('session','ack','stage','stack_frames','busy','ready','prepared','error') if k in s})
        r['observations']=trace
        r['natural_completion']=a.natural
        r['minimum_frames']=a.minimum_frames
        if a.require_flash:
            r['flash_samples']=flash_samples;r['first_flash_failure']=flash_failure
            r['flash_stable']=bool(flash_samples and flash_samples[-1].get('sampled_ms',0)>flash_samples[0].get('sampled_ms',0) and flash_failure is None and all(flash_samples[-1].get(k)==v for k,v in {'flashReady':1,'flashPending':0,'flashRequested':1,'flashSampling':1,'flashFault':0,'flashError':0,'flashErrno':0}.items()))
        r['exposure_status']=i.property('camera','exposure_status');r['active_error']=i.property('error','active_error');r['mount']=one('awk \'$2=="/system"{print $4}\' /proc/mounts')
        target.write_text(json.dumps(r,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
        count=s.get('stack_frames')
        frames_ok=type(count) is int and (count>=2 if a.natural else count>=a.minimum_frames if a.minimum_frames is not None else count==a.frames)
        assert s.get('stage')=='complete' and frames_ok and s.get('ready') is True and not s.get('prepared') and not s.get('error'), 'Task not accepted as complete'
        assert r['exposure_status']=='0' and r['active_error']=='false' and r['mount'].split(',')[0]=='ro'
        if a.require_flash:assert r['flash_stable'],'Flash fault or final sampling disabled; same capture has completed, no retry sent'
        print('TASK_COMPLETE_IDLE_VERIFIED',a.tag,flush=True)

if __name__=='__main__':main()
