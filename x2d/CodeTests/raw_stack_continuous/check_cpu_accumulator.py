"""执行实际分段CPU累计分配/回收路径，故障分配器替身，无相机操作。"""
from pathlib import Path
import hashlib,json,os,subprocess
ROOT=Path(__file__).resolve().parents[3];HERE=Path(__file__).resolve().parent
SRC=ROOT/'x2d/CodeTests/pixel_shift_rgb'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 assert Path.cwd().resolve()==ROOT
 paths=[HERE/'native_stack_capture.c',HERE/'check_cpu_accumulator.c',Path(__file__),SRC/'raw_stack_core.c',SRC/'raw_stack_async.c',HERE/'raw_stack_dma.h']
 hashes={str(p.relative_to(ROOT)):sha(p) for p in paths}
 revision=hashlib.sha256(json.dumps(hashes,sort_keys=True).encode()).hexdigest()[:20]
 out=ROOT/'x2d/outputs/4.2.0/raw-stack/continuous-candidate/cpu-allocation-check'/revision;out.mkdir(parents=True,exist_ok=True)
 s=paths[0].read_text(encoding='utf8');body=s[s.index('static int pool_allocate(void){'):s.index('static int destination(')]
 (out/'test.c').write_text(paths[1].read_text(encoding='utf8').replace('/* PRODUCTION_CPU_ALLOCATION */',body),encoding='utf8')
 zig=ROOT/'.research-cache/x1d-1.25.0/toolchain/zig-windows-x86_64-0.13.0/zig.exe'
 env=dict(os.environ,TEMP=str(out),TMP=str(out),ZIG_LOCAL_CACHE_DIR=str(out/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(out/'zig-global'))
 cmd=[str(zig),'cc','-O2','-pthread','-DRS_ASYNC_DIRECT=1','-I',str(HERE),'-I',str(SRC),str(out/'test.c'),str(SRC/'raw_stack_core.c'),str(SRC/'raw_stack_async.c'),'-o',str(out/'test.exe')]
 r=subprocess.run(cmd,capture_output=True,text=True,env=env);assert not r.returncode,r.stderr
 r=subprocess.run([str(out/'test.exe')],capture_output=True,text=True,env=env);assert not r.returncode,r.stdout+r.stderr
 for p in paths:assert sha(p)==hashes[str(p.relative_to(ROOT))]
 (out/'report.json').write_text(json.dumps(dict(passed=True,source_sha256=hashes,stdout=r.stdout,hardware=False),indent=2),encoding='utf8');print(r.stdout)
if __name__=='__main__':main()
