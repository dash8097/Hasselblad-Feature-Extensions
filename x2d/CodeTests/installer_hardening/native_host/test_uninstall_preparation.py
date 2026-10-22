"""只执行提前准备退出函数的故障模型；不导入真实传输。"""
import ast
import base64
import hashlib
import json
from pathlib import Path
import re
import types
import unittest


def check(value,message):
    if not value:raise ValueError(message)


tree=ast.parse(Path(__file__).with_name('native_client_support.py').read_text(encoding='utf8'))
functions=[n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name in ('digest','prepare_uninstall')]
ns=dict(check=check,base64=base64,hashlib=hashlib,re=re,strict_json=json.loads,time=types.SimpleNamespace(sleep=lambda _:None))
exec(compile(ast.Module(body=functions,type_ignores=[]),'<actual-preparation>','exec'),ns)


class Fake:
    def __init__(self,prepared=True,held=True,busy=False):
        self.prepared=prepared;self.held=held;self.busy=busy;self.calls=[];self.log=[]
        self.absent=False;self.unknown=False;self.changed=False;self.never=False
        self.beat=0;self.session='12345678';self.pid='123';self.start='99'
        self.after_write=None;self.storage='0';self.identity_reads=0;self.replace_before=False
        self.removal={'targets':[{'path':'/system/bin/x2d-pixel-overclock/worker','allowed':['a'*64,'PROJECT_FILE']}]}
    def status(self,text):pass
    def property(self,service,name):
        return {'exposure_status':'0','active_error':'false','system_state':'1','storage_status':self.storage}[name]
    def one(self,cmd):
        self.calls.append(cmd)
        if cmd.startswith('p=/tmp/x2d-multishot-active'):return 'HELD' if self.held else 'FREE'
        if cmd.startswith('p=/tmp/x2d-pixel-overclock/status'):
            if self.absent:return 'ABSENT'
            self.beat+=1
            return base64.b64encode(json.dumps(dict(protocol=3,busy=self.busy,prepared=self.prepared,
                preparing=False,session=self.session,ack=0,shutter_pending=False,heartbeat=self.beat)).encode()).decode()
        if cmd=='pidof worker':
            self.identity_reads+=1
            if self.replace_before and self.identity_reads>1:self.start='100'
            return self.pid
        if cmd.startswith('readlink /proc/'):return '/system/bin/x2d-pixel-overclock/worker'
        if cmd.startswith('stat -Lc '):return '12:34\n12:34'
        if cmd.startswith('/system/xbin/busybox base64 /proc/'):
            return base64.b64encode((self.pid+' (worker) S '+'0 '*18+self.start+' 0\n').encode()).decode()
        if cmd.startswith('test ! -L /system/bin'):
            return ('b' if self.unknown else 'a')*64+'  /system/bin/x2d-pixel-overclock/worker'
        if cmd.startswith('d='):return 'READY'
        if cmd.startswith('cd '):
            if self.changed:raise RuntimeError('state changed')
            if not self.never:self.prepared=False;self.held=False
            if self.after_write:self.after_write(self)
            return 'DISARMED'
        raise AssertionError(cmd)
    def writes(self):return [s for s in self.calls if s.startswith('cd ')]


class Preparation(unittest.TestCase):
    def test_prepared_normal_release(self):
        f=Fake();ns['prepare_uninstall'](f)
        self.assertEqual(len(f.writes()),1)
        self.assertFalse(any('rmdir' in x or 'rm ' in x for x in f.calls))
        self.assertGreater(f.beat,1)
        self.assertTrue(all(len(c.encode())<=231 for c in f.calls))
        self.assertNotIn('heartbeat',f.writes()[0])
    def test_idle_no_writes(self):
        f=Fake(False,False);ns['prepare_uninstall'](f);self.assertEqual(f.writes(),[])
    def test_absent_no_lock(self):
        f=Fake(False,False);f.absent=True;ns['prepare_uninstall'](f);self.assertEqual(f.writes(),[])
    def test_busy_refused(self):
        f=Fake(busy=True)
        with self.assertRaises(ValueError):ns['prepare_uninstall'](f)
        self.assertEqual(f.writes(),[])
    def test_unknown_lock_refused(self):
        f=Fake(False,True)
        with self.assertRaises(ValueError):ns['prepare_uninstall'](f)
        self.assertEqual(f.writes(),[])
    def test_unknown_worker_refused(self):
        f=Fake();f.unknown=True
        with self.assertRaises(ValueError):ns['prepare_uninstall'](f)
        self.assertEqual(f.writes(),[])
    def test_changed_state_no_retry(self):
        f=Fake();f.changed=True
        with self.assertRaises(RuntimeError):ns['prepare_uninstall'](f)
        self.assertEqual(len(f.writes()),1)
    def test_stuck_does_not_remove_lock_or_resend(self):
        f=Fake();f.never=True
        with self.assertRaises(ValueError):ns['prepare_uninstall'](f)
        self.assertTrue(f.held);self.assertEqual(len(f.writes()),1)
    def test_worker_changed_before_write(self):
        f=Fake();f.replace_before=True
        with self.assertRaises(ValueError):ns['prepare_uninstall'](f)
        self.assertEqual(f.writes(),[])
    def test_worker_restarted_after_write(self):
        f=Fake();f.after_write=lambda s:setattr(s,'start','100')
        with self.assertRaisesRegex(ValueError,'replaced-after-disarm'):ns['prepare_uninstall'](f)
        self.assertEqual(len(f.writes()),1)
    def test_session_changed_during_release(self):
        f=Fake();f.after_write=lambda s:setattr(s,'session','87654321')
        with self.assertRaisesRegex(ValueError,'session-changed'):ns['prepare_uninstall'](f)
    def test_became_busy_during_release(self):
        f=Fake();f.after_write=lambda s:setattr(s,'busy',True)
        with self.assertRaises(ValueError):ns['prepare_uninstall'](f)
    def test_status_disappeared_after_disarm(self):
        f=Fake();f.after_write=lambda s:setattr(s,'absent',True)
        with self.assertRaisesRegex(ValueError,'disappeared'):ns['prepare_uninstall'](f)
    def test_storage_busy_after_lock_release(self):
        f=Fake();f.storage='1'
        with self.assertRaisesRegex(ValueError,'storage-not-ready'):ns['prepare_uninstall'](f)


if __name__=='__main__':unittest.main(verbosity=2)
