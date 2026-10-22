"""仅假 transport；不会导入真实 USB 或设备库。"""
from pathlib import Path
import base64,json,sys,tempfile,unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from native_adapter import EXE,Journal,NativeAdapter,Ticket,Unconfirmed,ReceiptError

ATTEMPT='1234567890abcdef';TX='11223344'
def final(t,primary=0,restore=0):
 fields=dict(operation=t.operation,attempt=t.attempt,primary_rc=str(primary),restore_rc=str(restore),final_rc=str(restore or primary),primary_step='none' if not primary else 'injected',manual_reboot_required='1')
 if t.operation=='install-stock':fields.update(manifest=t.manifest,firmware='4.2.0',feature_mask='255',files_complete='1')
 else:fields['transaction']=TX
 if t.operation=='data-cleanup':fields.update(mode=t.mode,outcome='complete')
 if t.operation=='payload-unpack':
  fields.pop('manual_reboot_required');fields.pop('transaction');fields.update(kind=t.kind,stage=t.stage,manifest=t.manifest,primary_errno='0',payload_ready='1')
 return 'NATIVE_TRANSACTION_V1\n'+''.join(k+'='+v+'\n' for k,v in fields.items())
def envelope(t,*,exit='0\n',receipt=True,primary=0,restore=0):
 texts=dict(claim=f'NATIVE_DISPATCH_V1\noperation={t.operation}\nattempt={t.attempt}\n',started=f'NATIVE_DISPATCH_STARTED_V1\noperation={t.operation}\nattempt={t.attempt}\npid=123\nstart=456\n',exit=exit,final=final(t,primary,restore) if receipt else None)
 return dict(protocol='NATIVE_TRANSACTION_READ_V1',operation=t.operation,stage=t.stage,attempt=t.attempt,files={k:dict(state='PRESENT' if v is not None else 'ABSENT',data=base64.b64encode(v.encode()).decode() if v is not None else '') for k,v in texts.items()})
class Fake:
 def __init__(self):self.context={};self.first_failure='';self.calls=[];self.reply=None;self.fail_send=False;self.uploads=[]
 def status(self,s):self.context['stage']=s
 def one(self,c):
  self.calls.append(c)
  if ' dispatch ' in c:
   if self.fail_send:raise OSError('SHELL_RECEIVE')
   parts=c.split();return 'NATIVE_DISPATCHED_V1 operation='+parts[2]+' attempt='+parts[-1]
  if ' transaction-read ' in c:
   if isinstance(self.reply,Exception):raise self.reply
   return json.dumps(self.reply)
  if ' stage-init ' in c:return 'NATIVE_STAGE_DIRECTORY_READY'
  raise AssertionError(c)
 def upload(self,*args):self.uploads.append(args)
 def diagnose(self):self.context={'reason':'full diagnostic','worker':'full'};self.first_failure='replaced';return 'all diagnostics'
 def verify_current(self):return 'FULL_CURRENT_VERSION_CHECK'
class Tests(unittest.TestCase):
 def test_read_during_rw_but_success_requires_ro(self):
  self.send(self.t);self.host.reply=envelope(self.t,exit=None,receipt=False)
  self.a.verify_reader=lambda _:True
  def not_readonly(_):raise RuntimeError('system rw')
  self.a.verify_bootstrap=not_readonly
  self.assertIsNone(self.a.observe(self.t))
  self.host.reply=envelope(self.t)
  with self.assertRaises(Unconfirmed):self.a.observe(self.t)
  self.a.verify_bootstrap=lambda _:True
  self.assertTrue(self.a.observe(self.t).completed)
 def test_install_network_no_replay(self):
  import hashlib
  from unittest.mock import patch
  data=b'complete archive';digest=hashlib.sha256(data).hexdigest()
  t=Ticket('payload-unpack','/data/local/tmp/x2d-rs1',ATTEMPT,manifest=digest,kind='install-stock')
  self.host.native_usb_network_upload=True
  with patch('native_network_upload.upload',return_value=True) as upload:
   self.a.upload(t,data,'native-install',digest)
   upload.assert_called_once();self.assertEqual(self.host.uploads,[])
  with patch('native_network_upload.upload',side_effect=TimeoutError()):
   with self.assertRaises(TimeoutError):self.a.upload(t,data,'native-install',digest)
   self.assertEqual(self.host.uploads,[])
  with patch('native_network_upload.upload',return_value=False):
   self.a.upload(t,data,'native-install',digest)
   self.assertEqual(len(self.host.uploads),1)
 def setUp(self):
  base=Path(__file__).resolve().parents[4]/'x2d/outputs/4.2.0/installer-hardening-r7/native-host/tests';base.mkdir(parents=True,exist_ok=True)
  self.tmp=tempfile.TemporaryDirectory(dir=base);self.host=Fake();self.a=NativeAdapter(self.host,Journal(self.tmp.name),lambda p:p==EXE);self.t=Ticket('uninstall','/data/local/tmp/x2d-un-'+TX,ATTEMPT)
 def tearDown(self):self.tmp.cleanup()
 def send(self,t=None):self.a.dispatch(t or self.t)
 def test_success(self):self.send();self.host.reply=envelope(self.t);self.assertTrue(self.a.observe(self.t).completed)
 def test_retained_old_final_waits_for_atomic_replacement(self):
  t=Ticket('retained-cleanup','/data/local/tmp/x2d-un-'+TX,ATTEMPT);self.send(t)
  self.host.reply=envelope(t,exit=None);old=final(t).replace(ATTEMPT,'abcdef0123456789');self.host.reply['files']['final']['data']=base64.b64encode(old.encode()).decode()
  self.assertIsNone(self.a.observe(t));self.assertNotIn('final',self.a.journal.load(t)['observed'])
  self.host.reply=envelope(t);self.assertTrue(self.a.observe(t).success)
 def test_retained_old_final_after_exit_refused(self):
  t=Ticket('retained-cleanup','/data/local/tmp/x2d-un-'+TX,ATTEMPT);self.send(t)
  self.host.reply=envelope(t);old=final(t).replace(ATTEMPT,'abcdef0123456789');self.host.reply['files']['final']['data']=base64.b64encode(old.encode()).decode()
  with self.assertRaises(Unconfirmed):self.a.observe(t)
 def test_retained_wrong_transaction_still_refused(self):
  t=Ticket('retained-cleanup','/data/local/tmp/x2d-un-'+TX,ATTEMPT);self.send(t)
  self.host.reply=envelope(t,exit=None);old=final(t).replace(ATTEMPT,'abcdef0123456789').replace(TX,'deadbeef');self.host.reply['files']['final']['data']=base64.b64encode(old.encode()).decode()
  with self.assertRaises(Unconfirmed):self.a.observe(t)
 def test_unpack_both_kinds(self):
  for i,kind in enumerate(('install-stock','uninstall')):
   t=Ticket('payload-unpack','/data/local/tmp/x2d-rs1' if i==0 else '/data/local/tmp/x2d-un-'+TX,f'{20+i:016x}',manifest='b'*64,kind=kind);self.send(t);self.host.reply=envelope(t);self.assertTrue(self.a.observe(t).completed)
 def test_unpack_wrong_kind_stage(self):
  with self.assertRaises(ReceiptError):self.send(Ticket('payload-unpack','/data/local/tmp/x2d-rs1',ATTEMPT,manifest='a'*64,kind='uninstall'))
 def test_unpack_digest_mismatch(self):
  t=Ticket('payload-unpack','/data/local/tmp/x2d-rs1',ATTEMPT,manifest='b'*64,kind='install-stock');self.send(t);self.host.reply=envelope(t);bad=final(t).replace('manifest='+t.manifest,'manifest='+'c'*64);self.host.reply['files']['final']['data']=base64.b64encode(bad.encode()).decode()
  with self.assertRaises(Unconfirmed):self.a.observe(t)
 def test_unpack_missing_ready(self):
  t=Ticket('payload-unpack','/data/local/tmp/x2d-rs1',ATTEMPT,manifest='b'*64,kind='install-stock');self.send(t);self.host.reply=envelope(t);bad=final(t).replace('payload_ready=1','payload_ready=0');self.host.reply['files']['final']['data']=base64.b64encode(bad.encode()).decode()
  with self.assertRaises(Unconfirmed):self.a.observe(t)
 def test_unpack_upload_binding(self):
  t=Ticket('payload-unpack','/data/local/tmp/x2d-rs1',ATTEMPT,manifest='b'*64,kind='install-stock')
  with self.assertRaises(ReceiptError):self.a.upload(t,b'bad','native-install','b'*64)
  self.assertEqual(self.host.uploads,[])
 def test_all_operations(self):
  for i,op in enumerate(('install-stock','uninstall','retained-cleanup','data-cleanup')):
   t=Ticket(op,'/data/local/tmp/x2d-rs1' if op=='install-stock' else '/data/local/tmp/x2d-'+('tidy-' if op=='data-cleanup' else 'un-')+TX,f'{i:016x}',manifest='a'*64 if op=='install-stock' else None,mode='before' if op=='data-cleanup' else None);self.send(t);self.host.reply=envelope(t);self.assertTrue(self.a.observe(t).completed)
 def test_send_disconnect_no_replay(self):
  self.host.fail_send=True
  with self.assertRaises(Unconfirmed):self.send()
  self.assertTrue(self.a.journal.load(self.t)['sent']);self.host.fail_send=False
  with self.assertRaises(FileExistsError):self.send()
  self.assertEqual(sum(' dispatch ' in c for c in self.host.calls),1)
 def test_resume_read_after_disconnect(self):
  self.host.fail_send=True
  with self.assertRaises(Unconfirmed):self.send()
  other=NativeAdapter(self.host,Journal(self.tmp.name),lambda p:True);self.host.reply=envelope(self.t);self.assertTrue(other.observe(self.t).success)
 def test_final_without_exit_pending(self):self.send();self.host.reply=envelope(self.t,exit=None);self.assertIsNone(self.a.observe(self.t))
 def test_exit_without_final_not_success(self):
  self.send();self.host.reply=envelope(self.t,receipt=False)
  with self.assertRaises(Unconfirmed):self.a.observe(self.t)
 def test_mismatch_quarantined(self):
  self.send();self.host.reply=envelope(self.t,exit='90\n')
  with self.assertRaises(Unconfirmed):self.a.observe(self.t)
  self.host.reply=envelope(self.t)
  with self.assertRaises(ReceiptError):self.a.observe(self.t)
 def test_primary_restore_separate(self):self.send();self.host.reply=envelope(self.t,exit='90\n',primary=44,restore=90);r=self.a.observe(self.t);self.assertEqual((r.primary_rc,r.restore_rc,r.final_rc),(44,90,90));self.assertIn('44',self.host.first_failure)
 def test_timeout_only_reads(self):
  self.send();self.host.reply=envelope(self.t,exit=None,receipt=False)
  with self.assertRaises(Unconfirmed):self.a.wait(self.t,polls=3,sleep=lambda _:None)
  self.assertEqual(sum(' dispatch ' in c for c in self.host.calls),1)
 def test_read_disconnect_stops_poll(self):
  self.send();self.host.reply=OSError('SHELL_RECEIVE')
  with self.assertRaises(Unconfirmed):self.a.wait(self.t,polls=3,sleep=lambda _:None)
  self.assertEqual(sum(' transaction-read ' in c for c in self.host.calls),1)
 def test_wrong_attempt(self):
  self.send();self.host.reply=envelope(self.t);self.host.reply['attempt']='0'*16
  with self.assertRaises(Unconfirmed):self.a.observe(self.t)
 def test_receipt_changes(self):
  self.send();self.host.reply=envelope(self.t,exit=None);self.a.observe(self.t);self.host.reply=envelope(self.t,exit='1\n',primary=1)
  with self.assertRaises(Unconfirmed):self.a.observe(self.t)
 def test_read_type_refused(self):
  self.send();self.host.reply=envelope(self.t);self.host.reply['files']['final']['state']='TYPE_REJECTED'
  with self.assertRaises(Unconfirmed):self.a.observe(self.t)
 def test_no_bootstrap_no_send(self):
  self.a.verify_bootstrap=lambda _:False
  with self.assertRaises(ReceiptError):self.send()
  self.assertEqual(self.host.calls,[])
 def test_illegal_stage(self):
  with self.assertRaises(ReceiptError):self.send(Ticket('uninstall','/data/local/tmp/x2d-un-'+TX+';evil',ATTEMPT))
  self.assertEqual(self.host.calls,[])
 def test_diagnose_preserves_first_error(self):
  self.host.first_failure='first';self.host.context['native_primary_rc']='44';self.assertEqual(self.a.diagnose(),'all diagnostics');self.assertEqual(self.host.first_failure,'first');self.assertEqual(self.host.context['worker'],'full');self.assertEqual(self.host.context['native_primary_rc'],'44')
 def test_preview_full_current_path(self):self.assertEqual(self.a.preview_current(),'FULL_CURRENT_VERSION_CHECK')
 def test_bootstrap_required_for_resume_read(self):
  self.send();self.a.verify_bootstrap=lambda _:False
  with self.assertRaises(ReceiptError):self.a.observe(self.t)
  self.assertFalse(any(' transaction-read ' in c for c in self.host.calls))
 def test_no_stage_mutation_after_send(self):
  self.send()
  with self.assertRaises(ReceiptError):self.a.prepare_stage(self.t)
  self.assertFalse(any(' stage-init ' in c for c in self.host.calls))
 def test_missing_dispatch_claim(self):
  self.send();self.host.reply=envelope(self.t);self.host.reply['files']['claim']={'state':'ABSENT','data':''}
  with self.assertRaises(Unconfirmed):self.a.observe(self.t)
 def test_deferred_is_not_completed(self):
  t=Ticket('data-cleanup','/data/local/tmp/x2d-tidy-'+TX,ATTEMPT,mode='before');self.send(t);self.host.reply=envelope(t)
  text=final(t).replace('outcome=complete','outcome=deferred-capture');self.host.reply['files']['final']['data']=base64.b64encode(text.encode()).decode();r=self.a.observe(t);self.assertTrue(r.success and r.deferred and not r.completed)
 def test_duplicate_json_key(self):
  self.send();self.host.one=lambda _:json.dumps(envelope(self.t))[:-1]+',"protocol":"NATIVE_TRANSACTION_READ_V1"}'
  with self.assertRaises(Unconfirmed):self.a.observe(self.t)
  self.assertTrue(self.a.journal.load(self.t)['quarantined'])
 def test_sent_journal_never_overwritten(self):
  self.send();path=Path(self.tmp.name)/(ATTEMPT+'.json');before=path.read_bytes();self.host.reply=envelope(self.t);self.a.observe(self.t);self.assertEqual(path.read_bytes(),before)
 def test_stale_observer_cannot_unquarantine(self):
  self.send();old=self.a.journal.load(self.t);new=self.a.journal.load(self.t);new['quarantined']=True;self.a.journal.save(self.t,new);self.a.journal.save(self.t,old);self.assertTrue(self.a.journal.load(self.t)['quarantined'])
 def test_concurrent_observed_digest_conflict(self):
  self.send();a=self.a.journal.load(self.t);b=self.a.journal.load(self.t);a['observed']['final']='a'*64;b['observed']['final']='b'*64;self.a.journal.save(self.t,a)
  with self.assertRaises(ReceiptError):self.a.journal.save(self.t,b)
 def test_host_journal_failure_prevents_send(self):
  def broken(_):raise OSError('fsync injected')
  self.a.journal.begin=broken
  with self.assertRaises(OSError):self.send()
  self.assertEqual(self.host.calls,[])
 def test_limit_counts_utf8_bytes(self):
  self.send();self.host.reply=envelope(self.t);self.host.reply['files']['final']['data']=base64.b64encode(('汉'*6000).encode()).decode()
  with self.assertRaises(Unconfirmed):self.a.observe(self.t)
  self.assertTrue(self.a.journal.load(self.t)['quarantined'])
 def test_starttime_zero_is_valid(self):
  self.send();self.host.reply=envelope(self.t);item=self.host.reply['files']['started'];text=base64.b64decode(item['data']).decode().replace('start=456','start=0');item['data']=base64.b64encode(text.encode()).decode();self.assertTrue(self.a.observe(self.t).success)
if __name__=='__main__':unittest.main(verbosity=2)
