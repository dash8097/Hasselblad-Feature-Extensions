"""真实 Qt Rectangle/Text 人脸框池；无设备，测试资产不交付。"""
from pathlib import Path
import ctypes,hashlib,json,os,re,subprocess,sys
sys.dont_write_bytecode=True
import build_processing_tests as b
def main():
 assert Path.cwd().resolve()==b.ROOT
 ctypes.windll.kernel32.SetErrorMode(0x8003)
 names=['face_draw_pool.cpp','face_draw_pool_test.cpp','face_draw_pool_build.py','abi_view_api.cpp','stock_factory.cpp','qt_stock_profile.cpp','qt_abi_profile.cpp']
 paths={b.HERE/n for n in names};pending=list(paths)
 while pending:
  p=pending.pop()
  for n in re.findall(r'#include "([^"]+)"',p.read_text(encoding='utf8')):
   q=b.HERE/n
   if q.exists() and q not in paths:paths.add(q);pending.append(q)
 identity={p.name:b.sha(p) for p in sorted(paths)};identity.update(compiler=b.sha(b.CXX),qt=b.sha(b.SDK/'bin/Qt6Quick.dll'))
 out=b.OUT/'native-ui/face-draw-pool'/hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()[:20];src=out/'private-source';src.mkdir(parents=True,exist_ok=False)
 for p in paths:(src/p.name).write_bytes(p.read_bytes())
 (out/'private-font.ttc').write_bytes(Path('C:/Windows/Fonts/msyh.ttc').read_bytes())
 env=dict(os.environ,TEMP=str(out),TMP=str(out),PATH=str(b.CXX.parent)+os.pathsep+str(b.SDK/'bin')+os.pathsep+os.environ['PATH'],QT_PLUGIN_PATH=str(b.SDK/'plugins'),QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',QML_DISABLE_DISK_CACHE='1')
 modules=['Quick','Qml','QmlModels','Gui','Network','Core'];inc=['-I'+str(b.SDK/'include'),'-I'+str(src)]
 for m in modules:inc+=['-I'+str(b.SDK/'include'/('Qt'+m)),'-I'+str(b.SDK/'include'/('Qt'+m)/'6.4.1'),'-I'+str(b.SDK/'include'/('Qt'+m)/'6.4.1'/('Qt'+m))]
 cmd=[b.CXX,'-std=c++17','-O2','-fno-builtin','-DQT_NO_KEYWORDS','-DQT_NO_DEBUG',*inc,*[src/n for n in names if n.endswith('.cpp')],*[b.SDK/'lib'/('libQt6'+m+'.a') for m in modules],'-Wl,--wrap=malloc','-Wl,--wrap=calloc','-o',out/'test.exe']
 r=subprocess.run(list(map(str,cmd)),env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=120);(out/'build.log').write_text(r.stdout+r.stderr,encoding='utf8');assert not r.returncode,r.stdout+r.stderr
 r=subprocess.run([str(out/'test.exe'),str(out)],env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=120);(out/'run.log').write_text(r.stdout+r.stderr,encoding='utf8');assert not r.returncode,r.stdout+r.stderr
 assert all(b.sha(p)==identity[p.name] for p in paths)
 (out/'report.json').write_text(json.dumps(dict(identity=identity,passed=True,author_tests=True,independent_approval=False,stdout=r.stdout,camera_requests=0,package_integrated=False),ensure_ascii=False,indent=2),encoding='utf8');print(out);print(r.stdout)
if __name__=='__main__':main()
