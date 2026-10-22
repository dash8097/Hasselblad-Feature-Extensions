"""完整包的25MP冷启动直编契约；只接受绑定当前源码的离线验证产物。"""
import hashlib,json,re
from pathlib import Path
TARGET='/system/lib64/libx2d_quarter_upstream.so'
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def validated_quarter(root):
 base=root/'x2d/outputs/4.2.0/pixel-shift-rgb/quarter-upstream-preload'
 record=json.loads((base/'sealed.json').read_text(encoding='utf8'))
 sources=root/'x2d/CodeTests/pixel_shift_rgb'
 for name,digest in record['sources'].items():assert sha(sources/name)==digest,('quarter source changed',name)
 for name,digest in record['proofs'].items():assert sha(base/name)==digest,('quarter proof changed',name)
 for name,digest in record['modules'].items():assert sha(base/name)==digest,('quarter artifact changed',name)
 assert record['passed'] and record['cold_start_preload'] and record['real_bionic_linker']
 return base,record
def service_init(data):
 wrapper=b'/system/lib64/libx2d_af_probe.so'
 assert data.count(wrapper)==1 and TARGET.encode() not in data
 return data.replace(wrapper,TARGET.encode()+b':'+wrapper)

def validate_runner_bindings(source_dir, storage_digest):
 for name in ('native_memory_runner.cpp','native_disk_runner.cpp','native_stack_runner.cpp'):
  text=(source_dir/name).read_text(encoding='utf8')
  matches=re.findall(r'quarter_(?:hash|sha)\s*=\s*"([0-9a-f]{64})"',text)
  assert matches==[storage_digest],('quarter runtime binding differs',name,matches,storage_digest)
