"""经完整包和就绪回读验证后，以实体快门输入启动已由UI准备的堆栈。"""
import argparse,json,re,sys,time
from package_ui import ROOT,OUT,PACKAGE,PACKAGE_ID,transport_lock,read_status

def main():
    p=argparse.ArgumentParser();p.add_argument('--case',required=True);p.add_argument('--method',type=int,choices=(0,1,2),required=True);p.add_argument('--bit',type=int,choices=(0,1),required=True);p.add_argument('--finish-observed',type=int,default=7);p.add_argument('--trace-admission',action='store_true');p.add_argument('--natural-completion',action='store_true');a=p.parse_args()
    assert sys.version_info[:2]==(3,11) and ROOT==__import__('pathlib').Path.cwd().resolve()
    assert re.fullmatch('[A-Za-z0-9_-]+',a.case)
    journal=OUT/'capture-actions.jsonl'
    prior=[json.loads(line) for line in journal.read_text(encoding='utf8').splitlines()] if journal.exists() else []
    assert not any(v.get('package')==PACKAGE_ID and v.get('case')==a.case and v.get('action')=='full-shutter-ui' for v in prior),'Input already intended; follow the same task, never repeat it'
    sys.path.insert(0,str(PACKAGE/'engine'));import windows_engine
    with transport_lock():
        w=windows_engine.Transport(PACKAGE);i=windows_engine.mac_patch.Installer(PACKAGE,run_batch=w.batch)
        assert 'NATIVE_UI_READY_CURRENT_REQUEST' in w.batch(['/system/bin/x2d-native-flow ui-ready'])[0]
        deadline=time.monotonic()+15
        while True:
            s=read_status(lambda c:w.batch([c])[0].strip());h=json.loads(w.batch(['cat /tmp/x2d-pixel-overclock/shutter-status.json'])[0])
            assert not s.get('busy') and not s.get('error'),'Unexpected active/failed task; no input'
            if s.get('ready') and s.get('prepared') and h.get('available') and not h.get('pending'):break
            assert time.monotonic()<deadline,'Preparation did not finish; no input'
            time.sleep(.5)
        arm=h['arm'].split();assert len(arm)==9 and arm[0]=='RS4' and arm[1]==s['session'] and arm[7:]==['START','END'] and int(arm[3])==a.method and arm[5]=='0'
        session=arm[1];ack=int(arm[2]);assert re.fullmatch('[0-9a-f]{8}',session) and ack>0
        assert i.property('camera','image_bit_depth')==str(a.bit)
        assert i.property('camera','exposure_status')=='0' and i.property('error','active_error')=='false'
        assert w.batch(['cat /sys/class/input/event3/device/name'])[0].strip()=='exposure_button'
        command='for e in "1 18 1" "0 0 0" w "1 18 0" "0 0 0";do if [ "$e" = w ];then sleep .08;else sendevent /dev/input/event3 $e;fi;done'
        with journal.open('a',encoding='utf8') as f:f.write(json.dumps(dict(time=time.time(),package=PACKAGE_ID,case=a.case,action='full-shutter-ui',session=session,expected_ack=ack,method=a.method,bit=a.bit,intent_before_input=True))+'\n');f.flush()
        w.batch([command])
        print('FULL_SHUTTER_UI_INPUT_SENT',session,ack,flush=True)
        if a.trace_admission:
            trace=[]
            for _ in range(6):
                item={'time':time.time()}
                for group,key in (('camera','exposure_status'),('storage','storage_status'),('error','active_error')):item[group+'/'+key]=i.property(group,key)
                state=read_status(lambda c:w.batch([c])[0].strip())
                item.update({n:state.get(n) for n in ('session','ack','stage','busy','stack_frames')});trace.append(item);print(json.dumps(item),flush=True)
            target=OUT/(a.case+'-admission-trace.json');assert not target.exists();target.write_text(json.dumps(trace,indent=2)+'\n',encoding='utf8')
    if a.natural_completion:
        print("NATURAL_COMPLETION_REQUESTED_NO_FINISH_TOUCH",session,ack,flush=True);return
    import package_finish
    sys.argv=['package_finish.py',session,str(ack),'--case',a.case,'--observed',str(a.finish_observed),'--timeout','90','--x','930','--y','490']
    package_finish.main()

if __name__=='__main__':main()
