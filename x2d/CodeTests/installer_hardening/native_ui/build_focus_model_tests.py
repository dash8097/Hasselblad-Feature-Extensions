"""AF-C 三态请求/回读/超时作者离线回归，不运行相机代理。"""
from pathlib import Path
import hashlib,json,os,subprocess
import build_processing_tests as b
def main():
 assert Path.cwd().resolve()==b.ROOT
 paths=[b.HERE/n for n in ('focus_model.hpp','focus_model.cpp','focus_model_test.cpp','build_focus_model_tests.py')]
 identity={p.name:b.sha(p) for p in paths};identity['compiler']=b.sha(b.CXX)
 out=b.OUT/'native-ui/focus-model'/hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()[:20];out.mkdir(parents=True,exist_ok=False)
 for p in paths:(out/p.name).write_bytes(p.read_bytes())
 env=dict(os.environ,TEMP=str(out),TMP=str(out),PATH=str(b.CXX.parent)+os.pathsep+os.environ['PATH'])
 cmd=[str(b.CXX),'-std=c++17','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',str(out/'focus_model.cpp'),str(out/'focus_model_test.cpp'),'-o',str(out/'test.exe')]
 r=subprocess.run(cmd,env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=120);(out/'build.log').write_text(r.stdout+r.stderr,encoding='utf8');assert not r.returncode,r.stdout+r.stderr
 r=subprocess.run([str(out/'test.exe')],env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=30);(out/'run.log').write_text(r.stdout+r.stderr,encoding='utf8');assert not r.returncode,r.stdout+r.stderr
 assert all(b.sha(p)==identity[p.name] for p in paths)
 (out/'report.json').write_text(json.dumps(dict(identity=identity,passed=True,author_tests=True,independent_approval=False,stdout=r.stdout,camera_requests=0,package_integrated=False),ensure_ascii=False,indent=2),encoding='utf8');print(out);print(r.stdout)
if __name__=='__main__':main()
