"""内部假传输编排测试；不加载 USB、不运行引导、不随包分发。"""
from pathlib import Path
import hashlib,sys,unittest
sys.path.insert(0,str(Path(__file__).resolve().parent.parent))
from native_transaction_contract import Receipt
from native_operations import NativeOperations,Payload,Unconfirmed

class Model:
    def __init__(self):
        self.events=[];self.fail=None;self.deferred=False;self.unconfirmed=False;self.ready=True
    def ensure(self):self.events.append('bootstrap');return True
    def prepare_stage(self,t):self.events.append(('prepare',t.operation,t.attempt))
    def upload(self,t,data,token,digest):self.events.append(('upload',token))
    def dispatch(self,t):
        self.events.append(('dispatch',t.operation,t.attempt))
        if self.fail==t.operation:raise Unconfirmed('ACK lost')
    def wait(self,t,**kwargs):
        self.events.append(('wait',t.operation))
        if self.unconfirmed:raise Unconfirmed('reply lost')
        fields={'files_complete':'1','manual_reboot_required':'1','payload_ready':'1' if self.ready else '0'}
        if t.operation=='data-cleanup':fields['outcome']='deferred-capture' if self.deferred else 'complete'
        return Receipt(t.operation,t.attempt,0,0,0,'complete',fields)
    def progress(self,value):self.events.append(('progress',value))

class OperationsTest(unittest.TestCase):
    def setUp(self):
        self.m=Model();self.counter=0
        def attempt():self.counter+=1;return f'{self.counter:016x}'
        self.ops=NativeOperations(self.m,self.m,self.m,attempt=attempt,transaction=lambda:'12345678')
    def payload(self,kind='install-stock'):
        data=b'fake archive, never dispatched to hardware'
        return Payload(kind,data,hashlib.sha256(data).hexdigest(),'b'*64)
    def calls(self):return [v[1] for v in self.m.events if isinstance(v,tuple) and v[0]=='dispatch']
    def test_install_orders_unpack_before_write_with_different_attempts(self):
        self.ops.install_files(self.payload())
        self.assertEqual(self.calls(),['payload-unpack','install-stock'])
        events=[v for v in self.m.events if isinstance(v,tuple) and v[0]=='dispatch']
        self.assertNotEqual(events[0][2],events[1][2])
        self.assertLess(self.m.events.index(('wait','payload-unpack')),self.m.events.index(events[1]))
    def test_unpack_ack_loss_never_submits_install(self):
        self.m.fail='payload-unpack'
        with self.assertRaises(Unconfirmed):self.ops.install_files(self.payload())
        self.assertEqual(self.calls(),['payload-unpack'])
    def test_install_ack_loss_never_retries(self):
        self.m.fail='install-stock'
        with self.assertRaises(Unconfirmed):self.ops.install_files(self.payload())
        self.assertEqual(self.calls(),['payload-unpack','install-stock'])
    def test_unpack_read_loss_never_submits_install(self):
        self.m.unconfirmed=True
        with self.assertRaises(Unconfirmed):self.ops.install_files(self.payload())
        self.assertEqual(self.calls(),['payload-unpack'])
    def test_uncommitted_payload_never_submits_install(self):
        self.m.ready=False
        with self.assertRaises(Exception):self.ops.install_files(self.payload())
        self.assertEqual(self.calls(),['payload-unpack'])
    def test_invalid_archive_never_bootstraps(self):
        good=self.payload();bad=Payload(good.kind,good.archive+b'x',good.archive_sha256,good.manifest_sha256)
        with self.assertRaises(Exception):self.ops.install_files(bad)
        self.assertEqual(self.m.events,[])
    def test_kind_mismatch_never_bootstraps(self):
        with self.assertRaises(Exception):self.ops.install_files(self.payload('uninstall'))
        self.assertEqual(self.m.events,[])
    def test_uninstall_uses_own_bundle_and_receipt(self):
        self.ops.uninstall_files(self.payload('uninstall'))
        self.assertEqual(self.calls(),['payload-unpack','uninstall'])
        self.assertIn(('upload','native-uninstall'),self.m.events)
    def test_deferred_pre_cleanup_stops(self):
        self.m.deferred=True
        with self.assertRaises(Unconfirmed):self.ops.data_cleanup('before')
        self.assertEqual(self.calls(),['data-cleanup'])
    def test_deferred_post_cleanup_remains_deferred(self):
        self.m.deferred=True
        self.assertTrue(self.ops.data_cleanup('installed').deferred)
        self.assertEqual(self.calls(),['data-cleanup'])
    def test_invalid_cleanup_is_local_only(self):
        with self.assertRaises(Exception):self.ops.data_cleanup('everything')
        self.assertEqual(self.m.events,[])
    def test_retained_cleanup_does_not_reinitialize_or_upload(self):
        self.ops.retained_cleanup('/data/local/tmp/x2d-un-12345678')
        self.assertEqual(self.calls(),['retained-cleanup'])
        self.assertFalse(any(isinstance(v,tuple) and v[0] in ('prepare','upload') for v in self.m.events))

if __name__=='__main__':unittest.main()
