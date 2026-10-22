"""实际生产纯16位/线程/分段/接收队列与连续段协议的离线检查。"""
from pathlib import Path
import hashlib,json,os,subprocess
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[2]
SRC=ROOT/'x2d/CodeTests/pixel_shift_rgb'
ZIG=ROOT/'.research-cache/x1d-1.25.0/toolchain/zig-windows-x86_64-0.13.0/zig.exe'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 assert Path.cwd().resolve()==ROOT
 names=['raw_stack_core.c','raw_stack_core.h','raw_stack_parallel.c','raw_stack_parallel.h','raw_stack_async.c','raw_stack_async.h','raw_stack_session.c','raw_stack_session.h','check_raw_stack_core.c','check_raw_stack_segments.c','check_raw_stack_async.c','check_raw_stack_dispatch.c']
 paths=[(HERE/n if (HERE/n).is_file() else SRC/n) for n in names]+[HERE/'check_burst_protocol.c',HERE/'native_stack_capture.c',Path(__file__)]
 hashes={p.relative_to(ROOT).as_posix():sha(p) for p in paths}
 identity=hashlib.sha256(json.dumps(hashes,sort_keys=True).encode()).hexdigest()[:20]
 out=ROOT/'x2d/outputs/4.2.0/raw-stack/continuous-candidate/checks'/identity;out.mkdir(parents=True,exist_ok=True)
 env=dict(os.environ,TEMP=str(out),TMP=str(out),ZIG_LOCAL_CACHE_DIR=str(out/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(out/'zig-global'))
 flags=[ZIG,'cc','-O2','-Wall','-Wextra','-Werror','-pthread','-I',SRC]
 logs={}
 def test(name,units,extra=()):
  exe=out/(name+'.exe')
  for args in ([*flags,*extra,*units,'-o',exe],[exe]):
   p=subprocess.run(list(map(str,args)),env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=150)
   assert not p.returncode,p.stdout+p.stderr
  logs[name]=p.stdout.strip()
 test('arithmetic',[SRC/n for n in ['check_raw_stack_core.c','check_raw_stack_segments.c','raw_stack_core.c','raw_stack_parallel.c','raw_stack_session.c']],['-DRS_SEGMENT_TEST_NO_MAIN=1'])
 test('async',[SRC/n for n in ['check_raw_stack_async.c','raw_stack_async.c','raw_stack_core.c']],['-DRS_ASYNC_HOST_CHECK=1'])
 test('async-direct',[SRC/n for n in ['check_raw_stack_async.c','raw_stack_async.c','raw_stack_core.c']],['-DRS_ASYNC_HOST_CHECK=1','-DRS_ASYNC_DIRECT=1'])
 capture=(HERE/'native_stack_capture.c').read_text(encoding='utf8')
 dispatch=capture[capture.index('static int frame_slot_available(void){'):capture.index('#ifdef X2D_QUARTER_MEMORY',capture.index('static int frame_slot_available(void){'))]
 harness=(HERE/'check_raw_stack_dispatch.c').read_text(encoding='utf8')
 harness=harness.replace('static RsCore core;','static unsigned aborting,burst_peak_refs;static uint64_t burst_epoch=1,burst_gap_ms;\nstatic RsCore core;')
 harness=harness.replace('assert(!exclusive);char *target=', 'assert(!exclusive);if(!strcmp(name,"burst.progress")){unsigned long long e,n,g;unsigned peak;assert(sscanf(data,"RSB1 %llu %llu %llu %u",&e,&n,&g,&peak)==4&&e==1&&n==completed);return 1;}char *target=')
 (out/'dispatch.c').write_text(harness.replace('/* PRODUCTION_DISPATCH_FRAME */',dispatch),encoding='utf8')
 test('dispatch',[out/'dispatch.c'])
 parse=capture[capture.index('static int parse_uint('):capture.index('static int path(')]
 burst=capture[capture.index('static unsigned burst_refs('):capture.index('/* 只记录拒收依据')]
 harness=(HERE/'check_burst_protocol.c').read_text(encoding='utf8').replace('/* PRODUCTION_PARSE_UINT */',parse).replace('/* PRODUCTION_BURST_PROTOCOL */',burst)
 (out/'burst.c').write_text(harness,encoding='utf8');test('burst',[out/'burst.c'])
 for p in paths:assert sha(p)==hashes[p.relative_to(ROOT).as_posix()]
 report=dict(passed=True,source_sha256=hashes,logs=logs,camera_requests=0,hardware_tested=False,full_factory_callback_order_tested=False)
 (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
 print(json.dumps(dict(directory=str(out),**report),ensure_ascii=False))
if __name__=='__main__':main()
