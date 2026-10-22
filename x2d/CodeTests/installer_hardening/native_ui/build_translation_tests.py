from pathlib import Path
import sys,os,hashlib,json,subprocess
sys.dont_write_bytecode=True
import build_processing_tests as b
names=['factory_translation.hpp','factory_translation.cpp','factory_translation_test.cpp','menu_discovery.hpp','abi_view_api.hpp','effect_view_core.hpp','protocol_text.hpp','local_heap.hpp']
identity={n:b.sha(b.HERE/n) for n in names}
out=b.OUT/'native-ui/translation'/hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()[:20];src=out/'private-source';src.mkdir(parents=True,exist_ok=False)
for n in names:(src/n).write_bytes((b.HERE/n).read_bytes())
env=dict(os.environ,TEMP=str(out),TMP=str(out),PATH=str(b.SDK/'bin')+os.pathsep+str(b.CXX.parent)+os.pathsep+os.environ['PATH'])
cmd=[b.CXX,'-std=c++17','-O2','-fno-builtin','-I'+str(src),'-I'+str(b.SDK/'include'),'-I'+str(b.SDK/'include/QtCore'),src/'factory_translation.cpp',src/'factory_translation_test.cpp',b.SDK/'lib/libQt6Core.a','-o',out/'test.exe']
r=subprocess.run(list(map(str,cmd)),env=env,capture_output=True,text=True,encoding='utf8',errors='replace');(out/'build.log').write_text(r.stdout+r.stderr,encoding='utf8');assert not r.returncode,r.stderr
r=subprocess.run([str(out/'test.exe')],env=env,capture_output=True,text=True,encoding='utf8',errors='replace');(out/'run.log').write_text(r.stdout+r.stderr,encoding='utf8');assert not r.returncode,r.stdout+r.stderr
(out/'report.json').write_text(json.dumps(dict(identity=identity,passed=True,stdout=r.stdout,actual_desktop_qt=True,hardware_operations=0),ensure_ascii=False,indent=2),encoding='utf8');print(out);print(r.stdout)
