from pathlib import Path
import sys,os,json,hashlib,subprocess
sys.dont_write_bytecode=True
import build_processing_tests as b
names=['native_feature_contract.h','test_native_feature_contract.c']
sources={n:(b.HERE/n).read_bytes() for n in names};identity={n:hashlib.sha256(v).hexdigest() for n,v in sources.items()};identity['builder']=b.sha(Path(__file__))
rev=hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()[:20];out=b.OUT/'native-ui/feature-contract'/rev;out.mkdir(parents=True,exist_ok=False)
for n,v in sources.items():(out/n).write_bytes(v)
env=dict(os.environ,TEMP=str(out),TMP=str(out),PATH=str(b.CXX.parent)+os.pathsep+os.environ['PATH'])
p=subprocess.run([str(b.CXX),'-Wall','-Wextra','-Werror','-O2',str(out/names[1]),'-o',str(out/'test.exe')],env=env,capture_output=True,text=True);assert not p.returncode,p.stderr
p=subprocess.run([str(out/'test.exe')],env=env,capture_output=True,text=True);report=dict(identity=identity,exit=p.returncode,stdout=p.stdout,stderr=p.stderr,hardware_operations=0,scope='strict full-configuration value parser; not the original GUI bootstrap');(out/'report.json').write_text(json.dumps(report,indent=2),encoding='utf8');print(out);print(p.stdout);assert not p.returncode
