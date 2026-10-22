"""显示factory固定原4.2 ABI生成+实际Qt QStringList/QString候选回归。"""
from pathlib import Path
import hashlib,json,os,subprocess,sys
sys.dont_write_bytecode=True
import build_processing_tests as b
def main():
 assert Path.cwd().resolve()==b.ROOT
 names=['screen_factory_profile.hpp','screen_factory_profile.cpp','screen_factory_test.cpp','screen_controls.hpp','screen_policy.hpp','factory_proxy.hpp','factory_proxy.cpp','factory_formatter.hpp','stock_factory.hpp','stock_factory.cpp','qt_stock_profile.hpp','qt_stock_profile.cpp','abi_view_api.hpp','menu_discovery.hpp','effect_view_core.hpp','local_heap.hpp','protocol_text.hpp','generate_screen_profile.py','screen_build_factory.py']
 paths=[b.HERE/n for n in names];ident={p.name:b.sha(p) for p in paths};ident.update(compiler=b.sha(b.CXX),qt=b.sha(b.SDK/'bin/Qt6Core.dll'))
 rev=hashlib.sha256(json.dumps(ident,sort_keys=True).encode()).hexdigest()[:20];out=b.OUT/'native-ui/screen-factory'/rev;src=out/'private-source';src.mkdir(parents=True,exist_ok=False)
 for p in paths:(src/p.name).write_bytes(p.read_bytes())
 sys.path.insert(0,str(b.ROOT/'x2d/tools'));from firmware_image import system_elf
 from generate_screen_profile import generate
 gui='16391452abdc69de9e0807e065c0f4ab3f1ccb5fc288f6fc4e6f5cb3bdca12e0';profile,evidence=generate(system_elf('/bin/camera-gui',gui));(out/'screen-profile.hpp').write_bytes(profile)
 env=dict(os.environ,TEMP=str(out),TMP=str(out),PATH=str(b.CXX.parent)+os.pathsep+str(b.SDK/'bin')+os.pathsep+os.environ['PATH']);modules=['Quick','Qml','QmlModels','Gui','Network','Core'];inc=['-I'+str(b.SDK/'include'),'-I'+str(src)]
 for m in modules:inc+=['-I'+str(b.SDK/'include'/('Qt'+m)),'-I'+str(b.SDK/'include'/('Qt'+m)/'6.4.1'),'-I'+str(b.SDK/'include'/('Qt'+m)/'6.4.1'/('Qt'+m))]
 cmd=[b.CXX,'-std=c++17','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-DQT_NO_KEYWORDS','-DQT_NO_DEBUG',*inc,*[src/n for n in names if n.endswith('.cpp')],*[b.SDK/'lib'/('libQt6'+m+'.a') for m in modules],'-o',out/'test.exe']
 def run(cmd,log):
  r=subprocess.run(list(map(str,cmd)),env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=120);(out/log).write_text(r.stdout+r.stderr,encoding='utf8');assert not r.returncode,r.stdout+r.stderr;return r.stdout
 run(cmd,'build.log');stdout=run([out/'test.exe'],'run.log');assert all(b.sha(p)==ident[p.name] for p in paths)
 (out/'report.json').write_text(json.dumps(dict(identity=ident,gui_sha256=gui,original_abi=evidence,stdout=stdout,author_tests=True,hardware_calls=0,gui_loaded=False,package_integrated=False),ensure_ascii=False,indent=2),encoding='utf8');print(out);print(stdout)
if __name__=='__main__':main()
