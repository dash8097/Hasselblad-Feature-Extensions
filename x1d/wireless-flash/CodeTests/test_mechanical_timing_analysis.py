"""合成记录验证解析边界与软件时间分段；不是实测数据。"""
from pathlib import Path
import sys
import unittest
sys.dont_write_bytecode=True
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'research'))
from analyze_mechanical_timing import HEADER,EVENT,FIELDS,parse,analyze,to_csv

def event(kind,at,**kwargs):
    values=dict.fromkeys(FIELDS,0)
    values.update(kind=kind,mono_us=at,trial=1,epoch=7,source=3,delay_us=7530)
    values.update(kwargs)
    return EVENT.pack(*(values[k] for k in FIELDS))

def file(*events):
    return HEADER.pack(0x31544248,1,72,len(events),0,len(events),0,0)+b''.join(events)

class TimingAnalysis(unittest.TestCase):
    def test_segments_and_no_cross_clock_subtraction(self):
        record=parse(file(event(2,1000040,observer_ns=1000000999,hardware_ticks=0xf123456789),
                          event(3,1000040,deadline_us=1007530),
                          event(4,1007630,deadline_us=1007530,detail=1),
                          event(6,1007632,sequence=44),
                          event(7,1007639,sequence=44,detail=80),
                          event(8,1009000,sequence=44,detail=1)))
        result=analyze(record)
        self.assertEqual(result['observer_to_worker']['max_us'],40)
        self.assertEqual(result['deadline_to_due']['max_us'],100)
        self.assertEqual(result['send_call_duration']['max_us'],7)
        self.assertEqual(result['submit_to_driver_reply']['max_us'],1361)
        self.assertIsNone(result['hardware_to_linux_latency_us'])
        self.assertIsNone(result['actual_flash_start_us'])
        self.assertFalse(result['compensation_applied'])
        self.assertIn('hardware_ticks',to_csv(record))

    def test_reply_is_bound_to_epoch_trial_sequence(self):
        result=analyze(parse(file(event(6,100,sequence=44),event(7,110,sequence=44,detail=80),
                                 event(8,130,sequence=44,epoch=8,detail=1))))
        self.assertEqual(result['submit_to_driver_reply'],{'count':0})

    def test_quantization_and_reversed_time(self):
        result=analyze(parse(file(event(2,1000,observer_ns=1000999),
                                 event(4,1000,deadline_us=1001,detail=1))))
        self.assertEqual(result['observer_to_worker']['max_us'],0)
        self.assertEqual(result['invalid_time_relations'],['due_before_deadline'])

    def test_reject_malformed(self):
        valid=file(event(2,1000))
        for broken in (b'',valid[:-1],valid+b'x',b'XXXX'+valid[4:],
                       HEADER.pack(0x31544248,1,72,1,1,1,0,0)+valid[32:],
                       file(event(99,1000)),file(event(2,1000,delay_us=7531))):
            with self.assertRaises(ValueError): parse(broken)

if __name__=='__main__': unittest.main()
