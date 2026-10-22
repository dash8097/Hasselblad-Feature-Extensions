"""只用假 USB transport 的引导断线/回执模型；没有设备导入。"""
from pathlib import Path
import base64,hashlib,json,re,sys,tempfile,unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from native_bootstrap import NativeBootstrap,EXE,STAGE,LOCK,TOYBOX_SHA,Unconfirmed
DATA=b'\x7fELF'+bytes(128);SHA=hashlib.sha256(DATA).hexdigest();OLD='a'*64
class Fake:
 def __init__(self):self.context={};self.first_failure='';self.calls=[];self.files={};self.ro=True;self.disconnect=None;self.final=None;self.fail_primary=0;self.fail_restore=0;self.cleanup_final=None;self.seal_fault=None
 def ensure_stage(self,stage):assert stage==STAGE
 def upload(self,data,stage,token,digest):assert hashlib.sha256(data).hexdigest()==digest;self.files[stage+'/'+token+'.gz']=(digest,'600')
 def one(self,c):
  self.calls.append(c);assert len(c.encode('ascii'))<=231
  if c=='command -v flock':return '/system/bin/flock'
  if c=='readlink -f /system/bin/flock':return '/system/bin/toybox'
  if c=='command -v zcat':return '/system/bin/zcat'
  if c=='readlink -f /system/bin/zcat':return '/system/bin/toybox'
  if c=='/system/xbin/busybox sha256sum /system/bin/toybox':return TOYBOX_SHA+'  /system/bin/toybox'
  if c=='(umask 077;set -C;: > '+LOCK+')':self.files[LOCK]=('lock','600');return ''
  if c=='/system/xbin/busybox stat -c %u:%a:%h:%s '+LOCK:return '0:600:1:0'
  if c=='cat /proc/mounts':return 'sys /system ext4 '+('ro' if self.ro else 'rw')+',nosuid,nodev 0 0'
  if c=='/system/xbin/busybox stat -f -c %a:%S /system':return getattr(self,'space','4096:4096')
  if 'bootstrap.cleanup.final.' in c:
   if self.cleanup_final is None:raise OSError('ABSENT')
   if c.endswith('.seal') and self.seal_fault=='missing':raise OSError('ABSENT')
   if c.endswith('.seal') and self.seal_fault=='wrong' and ' base64 ' in c:return base64.b64encode(b'0'*64).decode()
   if ' stat ' in c:return '0:600:1:'+str(64 if c.endswith('.seal') else len(self.cleanup_final))+':1:2:3:4'
   if ' base64 ' in c:return base64.b64encode(hashlib.sha256(self.cleanup_final.encode()).hexdigest().encode() if c.endswith('.seal') else self.cleanup_final.encode()).decode()
   return ''
  if c.startswith('p='):return 'FILE' if c.split(';')[0][2:] in self.files else 'ABSENT'
  if c.startswith('/system/xbin/busybox stat '):return '0:'+self.files[c.split()[-1]][1]+':1:132'
  if c.startswith('/system/xbin/busybox sha256sum '):return self.files[c.split()[-1]][0]+'  '+c.split()[-1]
  if c.startswith('{ flock '):
   p='/system/bin/'+c.split(';')[2].split('/')[-1];self.files[p]=(SHA,'755')
   if self.disconnect=='seed':raise OSError('SHELL_RECEIVE')
   return ''
  if ' bootstrap-commit ' in c:
   exe,_,attempt,old,new=c.split();primary=self.fail_primary;restore=self.fail_restore
   if not primary:self.files[EXE]=(new,'755');self.files.pop(exe)
   fields=dict(attempt=attempt,old=old,new=new,primary_step='injected' if primary else 'none',primary_errno='5' if primary else '0',primary_rc=str(primary),restore_rc=str(restore),final_rc=str(restore or primary),setup_ready='0' if primary else '1',retained='none' if primary or old=='absent' else 'x2d-native-setup.previous.'+attempt)
   self.final='NATIVE_BOOTSTRAP_V1\n'+''.join(k+'='+v+'\n' for k,v in fields.items())
   if self.disconnect=='commit':raise OSError('SHELL_RECEIVE')
   return ''
  if ' bootstrap-read ' in c:return json.dumps(dict(protocol='NATIVE_BOOTSTRAP_READ_V1',attempt=c.split()[-1],state='PRESENT' if self.final else 'ABSENT',data=base64.b64encode(self.final.encode()).decode() if self.final else ''))
  if ' bootstrap-cleanup ' in c:
   _,_,attempt,new,old,previous=c.split();self.files.pop(EXE,None)
   fields=dict(attempt=attempt,new=new,old=old,previous=previous,primary_step='none',primary_errno='0',primary_rc='0',restore_rc='0',final_rc='0',removed='1',expected='1',tools_removed='1')
   self.cleanup_final='NATIVE_BOOTSTRAP_CLEANUP_V1\n'+''.join(k+'='+v+'\n' for k,v in fields.items())
   if self.disconnect=='cleanup':raise OSError('SHELL_RECEIVE')
   return ''
  raise AssertionError(c)
class Tests(unittest.TestCase):
 def setUp(self):
  root=Path(__file__).resolve().parents[4]/'x2d/outputs/4.2.0/installer-hardening-r7/native-host/bootstrap-tests';root.mkdir(parents=True,exist_ok=True);self.temp=tempfile.TemporaryDirectory(dir=root);self.host=Fake();self.boot=NativeBootstrap(self.host,self.temp.name,DATA,SHA,(OLD,))
 def tearDown(self):self.temp.cleanup()
 def test_absent_install(self):self.assertTrue(self.boot.ensure());self.assertTrue(self.boot.verify(EXE));self.assertEqual(sum(' bootstrap-commit ' in c for c in self.host.calls),1)
 def test_compressed_upload_has_separate_digest(self):
  import gzip
  self.assertEqual(gzip.decompress(self.boot.wire_data),DATA)
  self.assertNotEqual(self.boot.wire_digest,SHA)
  self.boot.ensure();self.assertEqual(self.host.files[self.boot.source][0],self.boot.wire_digest)
 def test_reserved_space_is_not_available(self):
  self.host.space='0:4096'
  with self.assertRaisesRegex(Unconfirmed,'bootstrap-system-space-insufficient'):self.boot.ensure()
  self.assertNotIn(self.boot.source,self.host.files);self.assertFalse(self.boot.path.exists())
 def test_invalid_space_reply_rejected(self):
  self.host.space='2600:0'
  with self.assertRaisesRegex(Unconfirmed,'bootstrap-space-framing'):self.boot.ensure()
  self.assertNotIn(self.boot.source,self.host.files)
 def test_verified_seed_does_not_need_copy_space(self):
  self.host.space='0:4096';self.host.files[self.boot.seed]=(SHA,'755');self.assertTrue(self.boot.ensure())
 def test_known_update(self):self.host.files[EXE]=(OLD,'755');self.assertTrue(self.boot.ensure())
 def test_current_no_mutation(self):self.host.files[EXE]=(SHA,'755');self.assertTrue(self.boot.ensure());self.assertFalse(any(c.startswith('{ flock ') or ' bootstrap-commit ' in c for c in self.host.calls))
 def test_unknown_target(self):
  self.host.files[EXE]=('c'*64,'755')
  with self.assertRaises(Unconfirmed):self.boot.ensure()
  self.assertFalse(any(c.startswith('{ flock ') for c in self.host.calls))
 def test_unknown_seed_prefix_collision(self):
  self.host.files[self.boot.seed]=('b'*64,'755')
  with self.assertRaises(Unconfirmed):self.boot.ensure()
 def test_seed_response_loss_never_rewrites(self):
  self.host.disconnect='seed'
  with self.assertRaises(Unconfirmed):self.boot.ensure()
  with self.assertRaises(Unconfirmed):self.boot.ensure()
  self.assertEqual(sum(c.startswith('{ flock ') for c in self.host.calls),1);self.assertFalse(any(' bootstrap-commit ' in c for c in self.host.calls))
 def test_commit_response_loss_can_read(self):
  self.host.disconnect='commit'
  with self.assertRaises(Unconfirmed):self.boot.ensure()
  self.assertTrue(self.boot.ensure());self.assertEqual(sum(' bootstrap-commit ' in c for c in self.host.calls),1)
 def test_final_missing_not_success_even_target_matches(self):
  self.host.disconnect='commit'
  with self.assertRaises(Unconfirmed):self.boot.ensure()
  self.host.final=None
  with self.assertRaises(Unconfirmed):self.boot.ensure()
 def test_readonly_missing(self):
  self.host.ro=False
  with self.assertRaises(Unconfirmed):self.boot.ensure()
  self.assertEqual(len(self.host.calls),1)
 def test_primary_and_recovery(self):
  self.host.fail_primary=44;self.host.fail_restore=90
  with self.assertRaises(Unconfirmed):self.boot.ensure()
  self.assertEqual(self.host.context['native_primary_rc'],'44');self.assertEqual(self.host.context['native_restore_rc'],'90')
 def test_wrong_attempt_rejected(self):
  self.host.disconnect='commit'
  with self.assertRaises(Unconfirmed):self.boot.ensure()
  self.host.final=self.host.final.replace('attempt=', 'attempt=wrong')
  with self.assertRaises(Unconfirmed):self.boot.ensure()
 def test_completed_then_stage_removed(self):
  self.boot.ensure();self.host.final=None;self.assertTrue(self.boot.ensure());self.assertEqual(sum(' bootstrap-commit ' in c for c in self.host.calls),1)
 def test_completed_proof_does_not_skip_current_hash(self):
  self.boot.ensure();self.host.files[EXE]=(OLD,'755')
  with self.assertRaises(Unconfirmed):self.boot.ensure()
 def test_bad_mode_no_execution(self):
  self.host.files[self.boot.seed]=(SHA,'644')
  with self.assertRaises(Unconfirmed):self.boot.ensure()
 def test_existing_valid_seed_reuse(self):
  self.host.files[self.boot.seed]=(SHA,'755');self.assertTrue(self.boot.ensure());self.assertFalse(any(c.startswith('{ flock ') for c in self.host.calls))
 def test_command_limit_and_traps(self):
  self.boot.ensure();c=next(c for c in self.host.calls if c.startswith('{ flock '));self.assertLessEqual(len(c),231);self.assertIn("trap 'r=$?;mount -oremount,ro $s||r=90;exit $r' 0",c);self.assertIn("trap 'exit 143' 1 2 15",c)
 def test_cleanup_success_no_rebootstrap(self):
  self.boot.ensure();self.assertTrue(self.boot.cleanup())
  with self.assertRaises(Unconfirmed):self.boot.ensure()
  self.assertEqual(sum(' bootstrap-commit ' in c for c in self.host.calls),1)
 def test_cleanup_ack_loss_readonly_resume(self):
  self.boot.ensure();self.host.disconnect='cleanup'
  with self.assertRaises(Unconfirmed):self.boot.cleanup()
  checkpoint=len(self.host.calls);self.assertTrue(self.boot.cleanup())
  self.assertFalse(any(' bootstrap-' in c for c in self.host.calls[checkpoint:]));self.assertEqual(sum(' bootstrap-cleanup ' in c for c in self.host.calls),1)
 def test_cleanup_missing_receipt_never_replay(self):
  self.boot.ensure();self.host.disconnect='cleanup'
  with self.assertRaises(Unconfirmed):self.boot.cleanup()
  self.host.cleanup_final=None
  with self.assertRaises(Unconfirmed):self.boot.cleanup()
  self.assertEqual(sum(' bootstrap-cleanup ' in c for c in self.host.calls),1)
 def test_cleanup_wrong_receipt_rejected(self):
  self.boot.ensure();self.host.disconnect='cleanup'
  with self.assertRaises(Unconfirmed):self.boot.cleanup()
  self.host.cleanup_final=self.host.cleanup_final.replace('new='+SHA,'new='+OLD)
  with self.assertRaises(Unconfirmed):self.boot.cleanup()
 def test_cleanup_reused_current_without_install_journal(self):
  self.host.files[EXE]=(SHA,'755');self.assertTrue(self.boot.ensure());self.assertTrue(self.boot.cleanup())
 def test_cleanup_missing_seal_never_success(self):
  self.boot.ensure();self.host.seal_fault='missing'
  with self.assertRaises(Unconfirmed):self.boot.cleanup()
  self.assertEqual(sum(' bootstrap-cleanup ' in c for c in self.host.calls),1)
 def test_cleanup_wrong_seal_never_success(self):
  self.boot.ensure();self.host.seal_fault='wrong'
  with self.assertRaises(Unconfirmed):self.boot.cleanup()
if __name__=='__main__':unittest.main(verbosity=2)
