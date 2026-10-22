"""实际原生QQuickImage SVG vs 私有旧p69 oracle，40帧及生命周期。"""
from pathlib import Path
import ctypes,hashlib,json,os,subprocess,sys
import build_processing_tests as b
from prepare_native_animation_oracle import prepare

def main():
 assert Path.cwd().resolve()==b.ROOT;ctypes.windll.kernel32.SetErrorMode(0x8003);oracle=prepare()
 names=['native_animation_view.hpp','native_animation_view.cpp','generate_animation_image_profile.py','shutter_animation_core.hpp','shutter_animation_core.cpp','native_animation_test_support.hpp','test_native_animation_view.cpp','build_native_animation_view_tests.py','prepare_native_animation_oracle.py','qt_abi_profile.hpp','qt_abi_profile.cpp','qt_stock_profile.hpp','qt_stock_profile.cpp','qt_menu_profile.hpp','qt_menu_profile.cpp','signal_bridge.hpp','signal_bridge.cpp','qt_signal_profile.hpp','qt_signal_profile.cpp','stock_factory.hpp','stock_factory.cpp','menu_discovery.hpp','abi_view_api.hpp','abi_view_api.cpp','effect_view_core.hpp','local_heap.hpp']
 identity={n:b.sha(b.HERE/n) for n in names};identity['compiler']=b.sha(b.CXX);identity['qtquick']=b.sha(b.SDK/'bin/Qt6Quick.dll');identity['oracle']=b.sha(oracle/'report.json')
 svg=b.OUT/'native-ui/screen-svg-deps/sdk/6.4.1/mingw_64';identity['svgplugin']=b.sha(svg/'plugins/imageformats/qsvg.dll')
 out=b.OUT/'native-ui/native-animation-view'/hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()[:20];src=out/'private-source';src.mkdir(parents=True,exist_ok=False)
 for n in names:(src/n).write_bytes((b.HERE/n).read_bytes())
 mods=['Quick','Qml','QmlModels','Gui','Network','Core'];inc=['-I'+str(b.SDK/'include')]
 for m in mods:inc+=['-I'+str(b.SDK/'include'/('Qt'+m)),'-I'+str(b.SDK/'include'/('Qt'+m)/'6.4.1'),'-I'+str(b.SDK/'include'/('Qt'+m)/'6.4.1'/('Qt'+m))]
 env=dict(os.environ,TEMP=str(out),TMP=str(out),PATH=str(svg/'bin')+os.pathsep+str(b.CXX.parent)+os.pathsep+str(b.SDK/'bin')+os.pathsep+os.environ['PATH'],QT_PLUGIN_PATH=str(svg/'plugins')+os.pathsep+str(b.SDK/'plugins'),QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',QSG_RENDER_LOOP='basic',QML_DISABLE_DISK_CACHE='1',QML2_IMPORT_PATH=str(b.SDK/'qml'))
 cmd=[b.CXX,'-std=c++17','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-DQT_NO_DEBUG','-DQT_NO_KEYWORDS',*inc,*[src/n for n in names if n.endswith('.cpp')],*[b.SDK/'lib'/('libQt6'+m+'.a') for m in mods],'-o',out/'test.exe'];r=subprocess.run(list(map(str,cmd)),env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=120);(out/'build.log').write_text(r.stdout+r.stderr,encoding='utf8');assert not r.returncode,r.stdout+r.stderr
 r=subprocess.run([str(out/'test.exe'),str(oracle),str(out)],env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=180);(out/'run.log').write_text(r.stdout+r.stderr,encoding='utf8');print(out);print(r.stdout,r.stderr)
 report=dict(identity=identity,binary=b.sha(out/'test.exe'),exit=r.returncode,stdout=r.stdout,stderr=r.stderr,oracle=str(oracle),author_tests=True,independent_approval=False,original_GUI_executed=False,hardware_operations=0,package_integrated=False,scope='Qt6.4.1 software actual QQuickImage and Text, public SVG plugin. Fonts and old QML are private fixture only.')
 (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8');assert not r.returncode,r.stdout+r.stderr
 sys.path[:0]=[str(b.HERE.parent.parent/'temporary_af_speed_probe'),str(src)]
 from inspect_menu_resources import load_gui,resources
 from generate_animation_image_profile import generate
 gui=load_gui();generated,evidence=generate(gui,dict(resources(gui)));(out/'animation_image_profile.hpp').write_bytes(generated);(out/'original-profile.json').write_text(json.dumps(evidence,indent=2),encoding='utf8')
 assert all(b.sha(b.HERE/n)==identity[n] for n in names)
if __name__=='__main__':main()
