"""读取已导出的纯记录文件；不访问设备，不把跨时钟计数差当作延迟。"""
from pathlib import Path
import argparse
import csv
import io
import json
import struct

HEADER=struct.Struct('<8I')
EVENT=struct.Struct('<4Q10I')
FIELDS=('mono_us','observer_ns','hardware_ticks','deadline_us','kind','trial','epoch',
        'source','delay_us','sequence','detail','timer_control','sync_status','clear_flags')
KINDS={1:'config',2:'sample',3:'accept',4:'due',5:'skip',6:'submit_begin',
       7:'submit_end',8:'driver_reply',9:'driver_failure',10:'cancel'}

def parse(data):
    if len(data)<HEADER.size: raise ValueError('Truncated timing header')
    magic,version,stride,count,overwritten,lo,hi,reserved=HEADER.unpack_from(data)
    total=lo|(hi<<32)
    if (magic!=0x31544248 or version!=1 or stride!=EVENT.size or count>2048 or reserved or
        len(data)!=HEADER.size+count*stride or count!=min(total,2048) or overwritten!=min(total-count,0xffffffff)):
        raise ValueError('Invalid timing layout or retention counters')
    events=[]
    for i in range(count):
        e=dict(zip(FIELDS,EVENT.unpack_from(data,HEADER.size+i*stride)))
        if e['kind'] not in KINDS or e['source']>=7 or e['delay_us']>5000000 or e['delay_us']%10:
            raise ValueError('Invalid timing event')
        e['kind_name']=KINDS[e['kind']]
        events.append(e)
    return {'total':total,'retained':count,'overwritten':overwritten,'events':events}

def spread(values):
    if not values: return {'count':0}
    values=sorted(values)
    return {'count':len(values),'min_us':values[0],'median_us':values[len(values)//2],
            'max_us':values[-1],'range_us':values[-1]-values[0]}

def analyze(record):
    hops=[]; wakes=[]; calls=[]; replies=[]; failures=0
    starts={}; ends={}; accepts={}; due=[]; malformed=[]
    for e in record['events']:
        k=e['kind']; trial=(e['epoch'],e['trial']); key=trial+(e['sequence'],)
        if k==2:
            # 与 worker 同样向下取整到 us；保留 CSV 中原始 ns。
            dt=e['mono_us']-e['observer_ns']//1000
            if dt<0: malformed.append('worker_before_observer')
            else: hops.append(dt)
        elif k==3: accepts[trial]=e
        elif k==4:
            if e['detail']==1:
                dt=e['mono_us']-e['deadline_us']
                if dt<0: malformed.append('due_before_deadline')
                else: wakes.append(dt)
                due.append(e)
        elif k==6: starts[key]=e['mono_us']
        elif k==7:
            if key in starts:
                dt=e['mono_us']-starts[key]
                if dt<0: malformed.append('submit_time_reversed')
                else: calls.append(dt)
            if e['detail']!=0xffffffff: ends[key]=e['mono_us']
        elif k==8:
            if key in ends:
                dt=e['mono_us']-ends[key]
                if dt<0: malformed.append('reply_before_submit')
                else: replies.append(dt)
            if e['detail']!=1: failures+=1
        elif k==9: failures+=1
    by_setting={}
    for e in due:
        setting=(e['source'],e['delay_us'])
        by_setting.setdefault(setting,[]).append(e['mono_us']-e['deadline_us'])
    return {'retained':record['retained'],'overwritten':record['overwritten'],
            'observer_to_worker':spread(hops),'deadline_to_due':spread(wakes),
            'send_call_duration':spread(calls),'submit_to_driver_reply':spread(replies),
            'deadline_by_setting':[{'source':s,'delay_us':d,**spread(v)} for (s,d),v in sorted(by_setting.items())],
            'driver_failure_records':failures,'invalid_time_relations':malformed,
            'hardware_to_linux_latency_us':None,'actual_flash_start_us':None,
            'clock_mapping_available':False,'compensation_applied':False,
            'limitations':['硬件计数与 Linux 时钟未校准，不输出两者相减的消息延迟。',
                           '驱动回复包含芯片等待与下次准备，不是无线起发或闪灯发光时间。',
                           '记录本身的开销和实际发光未测量；不能由本文件证明全部物理抖动来源。']}

def to_csv(record):
    output=io.StringIO(newline='')
    writer=csv.DictWriter(output,fieldnames=FIELDS+('kind_name',))
    writer.writeheader(); writer.writerows(record['events'])
    return output.getvalue()

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('input',type=Path)
    p.add_argument('--output',required=True,type=Path)
    args=p.parse_args()
    root=Path(__file__).resolve().parents[3]
    for path in (args.input,args.output):
        if not path.resolve().is_relative_to(root): raise SystemExit('Workspace path required')
    args.output.mkdir(parents=True,exist_ok=True)
    record=parse(args.input.read_bytes()); result=analyze(record)
    (args.output/'timing.csv').write_text(to_csv(record),encoding='utf-8-sig',newline='')
    (args.output/'summary.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(result,ensure_ascii=False))
