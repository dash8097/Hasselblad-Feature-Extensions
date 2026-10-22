"""固定一次 setup/payload，编译并组装内部双包；不发布，不操作硬件。"""
import argparse,concurrent.futures,hashlib,json,os,subprocess,sys
from pathlib import Path
import build_native_client as client
HERE=Path(__file__).resolve().parent
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--payload',type=Path,required=True);parser.add_argument('--setup',type=Path,required=True);parser.add_argument('--basic-payload',type=Path);parser.add_argument('--basic-setup',type=Path);parser.add_argument('--previous-setup',type=Path,action='append',default=[]);args=parser.parse_args()
 assert Path.cwd().resolve()==client.ROOT and sys.version_info[:2]==(3,11)
 payload,setup=args.payload.resolve(),args.setup.resolve();assert payload.is_relative_to(client.R7) and setup.is_relative_to(client.R7)
 names=('build_dual_clients.py','build_native_client.py','profile_ui.py','build_native_modules.py','assemble_native_client.py','build_checked_start.py','build_mac_runtime.py','build_windows_runtime.py','package_guard.hpp','windows_checked_start.cpp','mac_checked_start.cpp')
 identity=dict(payload=sha(payload/'report.json'),setup=sha(setup),builders={n:sha(HERE/n) for n in names})
 identity['client_modules']={n:sha(HERE/n) for n in ('native_adapter.py','native_network_upload.py','native_bootstrap.py','native_client_support.py','native_operations.py','native_client_methods.py','native_receipt_cleanup.py')}
 assert (args.basic_payload is None)==(args.basic_setup is None)
 if args.basic_payload:
  assert args.basic_payload.resolve().is_relative_to(client.R7) and args.basic_setup.resolve().is_relative_to(client.R7)
  identity['basic_payload']=sha(args.basic_payload/'report.json');identity['basic_setup']=sha(args.basic_setup)
 identity['previous_setups']=[sha(p) for p in args.previous_setup]
 rev=hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()[:20];out=client.R7/'native-host/dual-build'/rev
 out.mkdir(parents=True,exist_ok=False);sources={p:client.build(payload,setup,p,args.previous_setup,args.basic_payload,args.basic_setup) for p in ('windows','mac')}
 def run(platform):
  env=dict(os.environ,TEMP=str(out),TMP=str(out),PYTHONDONTWRITEBYTECODE='1')
  def command(script,arguments,log):
   p=subprocess.run([sys.executable,'-X','utf8','-B',str(HERE/script),*map(str,arguments)],env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=1800)
   (out/(platform+'-'+log+'.txt')).write_text(p.stdout+p.stderr,encoding='utf8');assert not p.returncode,p.stdout+p.stderr
   result=json.loads(p.stdout.strip().splitlines()[-1]);return Path(result['directory'])
  compiled=command('build_native_modules.py',['--source',sources[platform]/'source','--platform',platform],'compile')
  assembled=command('assemble_native_client.py',['--sources',sources[platform],'--compiled',compiled],'assemble')
  return dict(sources=str(sources[platform]),compiled=str(compiled),assembled=str(assembled))
 with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
  futures={p:pool.submit(run,p) for p in sources};results={};errors={}
  for p,f in futures.items():
   try:results[p]=f.result()
   except Exception as e:errors[p]=str(e)
 assert all(sha(HERE/n)==digest for n,digest in identity['builders'].items()),'流水线期间构建代码变化'
 assert all(sha(HERE/n)==digest for n,digest in identity['client_modules'].items()),'流水线期间客户端模块变化'
 report=dict(identity=identity,results=results,errors=errors,release_ready=False,camera_requests=0,published=False,
             qualification='仅内部组包，所有目标完成后仍须独立checker和完整原生UI门禁')
 (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
 print(json.dumps(dict(directory=str(out),results=results,errors=errors,release_ready=False),ensure_ascii=False));assert not errors
if __name__=='__main__':main()
