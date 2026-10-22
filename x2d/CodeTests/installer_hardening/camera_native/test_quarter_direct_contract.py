"""防止高级直编加载项丢失、重复接入或被归入基础档。"""
import sys,unittest,tempfile
from pathlib import Path
import quarter_direct_contract as q
sys.path.insert(0,str(Path(__file__).resolve().parent.parent))
import package_profiles as p
class ContractTests(unittest.TestCase):
 def test_preserve_other_preloads(self):
  old=b'service camera-service /system/bin/camera-service\n    setenv LD_PRELOAD /system/lib64/libx2d_af_probe.so:/system/lib64/libx2d_face_lock.so\n'
  result=q.service_init(old)
  self.assertEqual(result.replace(q.TARGET.encode()+b':',b''),old)
  self.assertEqual(result.count(b'LD_PRELOAD'),1)
  with self.assertRaises(AssertionError):q.service_init(result)
 def test_advanced_only(self):
  rows=[dict(path=q.TARGET),dict(path='/system/lib64/libx2d_af_base0.so')]
  self.assertEqual(p.selected_targets(rows,p.BASIC),rows[1:])
  self.assertEqual(p.selected_targets(rows,p.ADVANCED),rows)
 def test_missing_or_ambiguous_init_rejected(self):
  for data in [b'',b'/system/lib64/libx2d_af_probe.so '*2]:
   with self.assertRaises(AssertionError):q.service_init(data)
 def test_frozen_runner_storage_binding(self):
  root=Path(__file__).resolve().parents[4]
  out=root/'x2d/outputs/4.2.0/installer-hardening-r7/quarter-binding-test'
  out.mkdir(exist_ok=True)
  digest='a'*64
  with tempfile.TemporaryDirectory(dir=out) as d:
   p=Path(d)
   for n in ('native_memory_runner.cpp','native_disk_runner.cpp','native_stack_runner.cpp'):
    (p/n).write_text('static const char*quarter_hash="'+digest+'";')
   q.validate_runner_bindings(p,digest)
   for n in ('native_memory_runner.cpp','native_disk_runner.cpp','native_stack_runner.cpp'):
    original=(p/n).read_text();(p/n).write_text(original.replace(digest,'b'*64))
    with self.assertRaises(AssertionError):q.validate_runner_bindings(p,digest)
    (p/n).write_text(original)
if __name__=='__main__':unittest.main()
