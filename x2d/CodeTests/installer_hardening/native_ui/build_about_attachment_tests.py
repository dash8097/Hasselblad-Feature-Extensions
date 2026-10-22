"""原生关于页自动发现，实际Qt右锚、信号、binding退出恢复。"""
from pathlib import Path
import ctypes,hashlib,json,os,subprocess,sys
import build_processing_tests as b
def main():
 assert Path.cwd().resolve()==b.ROOT
 ctypes.windll.kernel32.SetErrorMode(0x8003)
 names=['about_attachment.hpp','about_attachment.cpp','generate_about_profile.py','test_about_attachment.cpp','build_about_attachment_tests.py','about_row.hpp','about_row.cpp','about_display.hpp','about_display.cpp','factory_formatter.hpp','factory_formatter.cpp','qt_stock_profile.hpp','qt_stock_profile.cpp','qt_menu_profile.hpp','qt_menu_profile.cpp','qt_signal_profile.hpp','qt_signal_profile.cpp','qt_binding_profile.hpp','qt_binding_profile.cpp','qt_abi_profile.hpp','qt_abi_profile.cpp','stock_factory.hpp','stock_factory.cpp','signal_bridge.hpp','signal_bridge.cpp','binding_lease.hpp','binding_lease.cpp','tree_observer.hpp','tree_observer.cpp','menu_discovery.hpp','abi_view_api.hpp','abi_view_api.cpp','effect_view_core.hpp','local_heap.hpp','protocol_text.hpp']
 identity={n:b.sha(b.HERE/n) for n in names};identity['compiler']=b.sha(b.CXX);identity['qt']=b.sha(b.SDK/'bin/Qt6Quick.dll')
 out=b.OUT/'native-ui/about-attachment'/hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()[:20];out.mkdir(parents=True,exist_ok=False)
 for n in names:(out/n).write_bytes((b.HERE/n).read_bytes())
 env=dict(os.environ,TEMP=str(out),TMP=str(out),PATH=str(b.CXX.parent)+os.pathsep+str(b.SDK/'bin')+os.pathsep+os.environ['PATH'],QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',QML_DISABLE_DISK_CACHE='1',QML2_IMPORT_PATH=str(b.SDK/'qml'))
 modules=['Quick','Qml','QmlModels','Gui','Network','Core'];include=['-I'+str(b.SDK/'include')]
 for m in modules:include+=['-I'+str(b.SDK/'include'/('Qt'+m)),'-I'+str(b.SDK/'include'/('Qt'+m)/'6.4.1'),'-I'+str(b.SDK/'include'/('Qt'+m)/'6.4.1'/('Qt'+m))]
 cmd=[b.CXX,'-std=c++17','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-DQT_NO_DEBUG','-DQT_NO_KEYWORDS','-DQT_GUI_LIB','-DQT_CORE_LIB',*include,*[out/n for n in names if n.endswith('.cpp')],*[b.SDK/'lib'/('libQt6'+m+'.a') for m in modules],'-o',out/'test.exe']
 p=subprocess.run(list(map(str,cmd)),env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=120);(out/'build.log').write_text(p.stdout+p.stderr,encoding='utf8');assert not p.returncode,p.stdout+p.stderr
 p=subprocess.run([str(out/'test.exe')],env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=60);(out/'run.log').write_text(p.stdout+p.stderr,encoding='utf8');assert not p.returncode,p.stdout+p.stderr
 sys.path[:0]=[str(b.HERE.parent.parent/'temporary_af_speed_probe'),str(b.HERE.parent.parent/'pixel_shift_rgb')]
 from inspect_menu_resources import load_gui,resources
 from generate_about_profile import generate
 gui=load_gui();generated,evidence=generate(gui,dict(resources(gui)));(out/'about_profile.hpp').write_bytes(generated)
 assert all(b.sha(b.HERE/n)==identity[n] for n in names)
 (out/'report.json').write_text(json.dumps(dict(identity=identity,passed=True,author_test=True,original_4_2_evidence=evidence,binary=b.sha(out/'test.exe'),stdout=p.stdout,camera_operations=0,package_integrated=False),ensure_ascii=False,indent=2),encoding='utf8');print(out);print(p.stdout)
if __name__=='__main__':main()
