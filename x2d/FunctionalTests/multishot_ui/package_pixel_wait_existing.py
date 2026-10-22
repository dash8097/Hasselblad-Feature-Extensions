"""USB观察中断后只读等待同一像素任务，再交给既有验收器；从不补发输入。"""
import argparse, json, re, sys, time
from pathlib import Path
from package_ui import ROOT, OUT, PACKAGE, PACKAGE_ID, transport_lock, read_status
from package_pixel_shot import completion

def main():
    p=argparse.ArgumentParser()
    p.add_argument('tag');p.add_argument('--capture-case',required=True)
    p.add_argument('--bit',type=int,choices=(0,1),required=True)
    p.add_argument('--frames',type=int,choices=(6,16),required=True)
    p.add_argument('--keep',type=int,choices=(0,1),required=True)
    a=p.parse_args()
    assert sys.version_info[:2]==(3,11) and Path.cwd().resolve()==ROOT
    assert all(re.fullmatch('[A-Za-z0-9_-]+',x) for x in (a.tag,a.capture_case))
    baseline=json.loads((OUT/(a.capture_case+'.json')).read_text(encoding='utf8'))
    assert baseline['package']==PACKAGE_ID and baseline['phase']=='observing'
    session,ack=baseline['session'],baseline['ack']
    actions=[json.loads(x) for x in (OUT/'capture-actions.jsonl').read_text().splitlines()]
    assert actions[-1]['case']==a.capture_case and actions[-1]['package']==PACKAGE_ID
    assert actions[-1]['session']==session and actions[-1]['ack']==ack
    target=OUT/(a.tag+'-observations.json');assert not target.exists()
    record=dict(package=PACKAGE_ID,session=session,ack=ack,input_sent=False,observations=[])
    sys.path.insert(0,str(PACKAGE/'engine'));import windows_engine
    with transport_lock():
        w=windows_engine.Transport(PACKAGE);one=lambda c:w.batch([c])[0].strip()
        end=time.monotonic()+600;previous=None
        while True:
            s=read_status(one)
            assert s.get('session')==session and s.get('ack')==ack,'Identity changed; no input'
            sample={k:s.get(k) for k in ('stage','busy','ready','prepared','error','frames','work_done','work_total')}
            if sample!=previous:
                record['observations'].append(dict(time=time.time(),**sample));previous=sample
                target.write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
                print(json.dumps(sample,ensure_ascii=False),flush=True)
            if completion(s,ack,a.frames,a.keep) or s.get('error'):break
            assert time.monotonic()<end,'Still waiting; no UI mutation'
            time.sleep(1)
    import package_pixel_continuation
    sys.argv=['package_pixel_continuation.py',a.tag,'--capture-case',a.capture_case,'--bit',str(a.bit),'--frames',str(a.frames),'--keep',str(a.keep)]
    package_pixel_continuation.main()

if __name__=='__main__':main()
