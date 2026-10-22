from pathlib import Path
import hashlib,json,subprocess,os
ROOT=Path(__file__).resolve().parents[3];HERE=Path(__file__).resolve().parent
paths=[HERE/'raw_stack_dma.h',HERE/'check_dma.c',Path(__file__)]
hashes={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
rev=hashlib.sha256(json.dumps(hashes,sort_keys=True).encode()).hexdigest()[:20]
out=ROOT/'x2d/outputs/4.2.0/raw-stack/continuous-candidate/dma-check'/rev;out.mkdir(parents=True,exist_ok=True)
zig=ROOT/'.research-cache/x1d-1.25.0/toolchain/zig-windows-x86_64-0.13.0/zig.exe'
env=dict(os.environ,TEMP=str(out),TMP=str(out),ZIG_LOCAL_CACHE_DIR=str(out/'local'),ZIG_GLOBAL_CACHE_DIR=str(out/'global'))
subprocess.run([str(zig),'cc','-O2','-Wall','-Wextra','-Werror',str(HERE/'check_dma.c'),'-o',str(out/'test.exe')],env=env,check=True)
r=subprocess.run([str(out/'test.exe')],capture_output=True,text=True);assert r.returncode==0,r.stdout+r.stderr
(out/'report.json').write_text(json.dumps(dict(passed=True,source_sha256=hashes,stdout=r.stdout),indent=2),encoding='utf-8');print(r.stdout)
