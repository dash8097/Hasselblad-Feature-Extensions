"""保护循环陈旧观察负例与脉冲时序/退出路径；纯host替身，不操作相机。"""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3];HERE=Path(__file__).resolve().parent
def sha(data):return hashlib.sha256(data).hexdigest()
def main():
 assert Path.cwd().resolve()==ROOT
 p=argparse.ArgumentParser();p.add_argument('--baseline-only',action='store_true');a=p.parse_args()
 legacy=ROOT/'x2d/CodeTests/subject_tracking/device_hybrid_test/x2d_eshutter_flash/front_button_trial_v3/hotshoe_resident.c'
 ordinary=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/esflash/a037abf7a4fc6e653db2/source/hotshoe_resident.c'
 files={n:(HERE/n).read_bytes() for n in ('native_hotshoe_resident.c','native_pulse_test_platform.h','test_native_pulse.c')}
 identity=dict(sources={n:sha(b) for n,b in files.items()},legacy=sha(legacy.read_bytes()),ordinary_baseline=sha(ordinary.read_bytes()),builder=sha(Path(__file__).read_bytes()),baseline_only=a.baseline_only)
 out=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/esflash-pulse'/sha(json.dumps(identity,sort_keys=True).encode())[:20];out.mkdir(parents=True,exist_ok=False)
 zig=ROOT/'.research-cache/x1d-1.25.0/toolchain/zig-windows-x86_64-0.13.0/zig.exe'
 env=dict(os.environ,TEMP=str(out),TMP=str(out),ZIG_LOCAL_CACHE_DIR=str(out/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(out/'zig-global'))
 def build(name,source):
  d=out/name;d.mkdir()
  (d/'native_hotshoe_resident.c').write_bytes(source.replace(b'resident_test_platform.h',b'native_pulse_test_platform.h'))
  for n in ('native_pulse_test_platform.h','test_native_pulse.c'):(d/n).write_bytes(files[n])
  exe=d/'test.exe';r=subprocess.run([str(zig),'cc','-target','x86_64-windows-gnu','-O2','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function',str(d/'test_native_pulse.c'),'-o',str(exe)],env=env,capture_output=True,text=True,timeout=180);assert not r.returncode,r.stderr
  return exe
 old=build('baseline',legacy.read_bytes());red=subprocess.run([str(old),'stale-guard'],capture_output=True,text=True,timeout=10)
 assert red.returncode==1 and 'STALE_IDLE_GUARD fault=1 low_writes=1' in red.stdout
 report=dict(identity=identity,baseline_red_reproduced=True,baseline_stdout=red.stdout,camera_requests=0,hardware_mocked=True)
 if not a.baseline_only:
  old_window=build('ordinary-baseline',ordinary.read_bytes().split(b'\n#include "native_parent_start.h"')[0]);window_red=subprocess.run([str(old_window),'ordinary-window'],capture_output=True,text=True,timeout=10)
  assert window_red.returncode==1 and 'result=-1 fault=1 errno=110' in window_red.stdout,window_red.stdout+window_red.stderr
  exe=build('candidate',files['native_hotshoe_resident.c']);r=subprocess.run([str(exe)],capture_output=True,text=True,timeout=15)
  window_green=subprocess.run([str(exe),'ordinary-window'],capture_output=True,text=True,timeout=10)
  report.update(scheduler_window_red_reproduced=True,scheduler_window_baseline_stdout=window_red.stdout,scheduler_window_candidate_stdout=window_green.stdout,candidate_stdout=r.stdout+r.stderr,passed=r.returncode==0 and 'NATIVE_PULSE_PASSED' in r.stdout and window_green.returncode==0 and 'result=0 fault=0' in window_green.stdout)
  (out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8');assert report['passed'],report['candidate_stdout']
 else:
  report['fix_validated']=False;(out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
 print(out);print(json.dumps(report))
if __name__=='__main__':main()
