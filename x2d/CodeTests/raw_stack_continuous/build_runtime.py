"""复用冻结原生运行时构建器，明确覆盖连续堆栈入口并绑定接收库摘要。"""
from pathlib import Path
import hashlib,json,sys
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[2]
NATIVE=ROOT/'x2d/CodeTests/installer_hardening/camera_native'
OUT=ROOT/'x2d/outputs/4.2.0/raw-stack/continuous-candidate/runtime'
sys.path.insert(0,str(NATIVE))
import build_flow
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 args=sys.argv[1:];assert len(args) in (0,2) and (not args or args[0]=='--native-ui-contract')
 contract=json.loads(Path(args[1]).read_text(encoding='utf8')) if args else None
 proof_path=ROOT/'x2d/outputs/4.2.0/raw-stack/continuous-candidate/capture/latest-build.json'
 proof=json.loads(proof_path.read_text(encoding='utf8'));artifact=ROOT/proof['artifact']['path']
 assert sha(artifact)==proof['artifact']['sha256']
 assert sha(HERE/'native_stack_capture.c')==proof['identity']['sources']['native_stack_capture.c']
 assert sha(ROOT/'x2d/CodeTests/pixel_shift_rgb/raw_stack_core.c')==proof['identity']['sources']['raw_stack_core.c']
 assert 'module_hash="'+sha(artifact)+'"' in (HERE/'native_stack_runner.cpp').read_text(encoding='utf8')
 old=build_flow.input_path
 build_flow.input_path=lambda n:HERE/n if n=='native_stack_runner.cpp' else old(n)
 sys.argv=[str(NATIVE/'build_flow.py'),'--profile','runtime','--hide-literals','--output',str(OUT),*args]
 build_flow.main()
 candidates=[]
 for p in (OUT/'revisions').glob('*/build-proof.json'):
  r=json.loads(p.read_text(encoding='utf8'))
  current=all(n=='native_ui_contract.hpp' or not build_flow.input_path(n).is_file() or sha(build_flow.input_path(n))==h for n,h in r['source_sha256'].items())
  if current and r['source_sha256']['native_stack_runner.cpp']==sha(HERE/'native_stack_runner.cpp') and r['build_identity'].get('native_ui_contract')==contract:candidates.append(p)
 assert len(candidates)==1
 report=dict(wrapper_sha256=sha(Path(__file__)),build_proof=str(candidates[0].relative_to(ROOT)),capture_build_proof=str(proof_path.relative_to(ROOT)),capture_sha256=sha(artifact),camera_requests=0,installed=False)
 (OUT/'candidate-binding.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
if __name__=='__main__':main()
