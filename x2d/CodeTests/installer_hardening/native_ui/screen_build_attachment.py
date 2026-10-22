"""固定 Qt 6.4.1 原控件资源、假代理，自动菜单挂接作者离线验证。"""
from pathlib import Path
import ctypes,hashlib,json,os,subprocess,shutil,sys
sys.dont_write_bytecode=True
import build_processing_tests as b

def main():
 assert Path.cwd().resolve()==b.ROOT
 ctypes.windll.kernel32.SetErrorMode(0x8003)
 names=['screen_key_profile.cpp','qt_key_profile.cpp','screen_attachment.cpp','screen_factory_profile.cpp','screen_controls_view.cpp','screen_controls.cpp','screen_policy.cpp','factory_proxy.cpp','binding_lease.cpp','signal_bridge.cpp','qt_binding_profile.cpp','qt_signal_profile.cpp','stock_factory.cpp','abi_view_api.cpp','qt_stock_profile.cpp','qt_abi_profile.cpp','qt_menu_profile.cpp','menu_discovery.cpp','screen_attachment_test.cpp']
 headers=['screen_key_profile.hpp','qt_key_profile.hpp','key_filter.hpp','key_mapping.hpp','protocol_text.hpp','screen_attachment.hpp','screen_factory_profile.hpp','factory_formatter.hpp','factory_proxy.hpp','screen_controls_view.hpp','screen_controls.hpp','screen_policy.hpp','binding_lease.hpp','signal_bridge.hpp','qt_binding_profile.hpp','qt_signal_profile.hpp','stock_factory.hpp','abi_view_api.hpp','qt_stock_profile.hpp','qt_abi_profile.hpp','qt_menu_profile.hpp','menu_discovery.hpp','effect_view_core.hpp','local_heap.hpp','screen_factory_test.cpp','screen_build_attachment.py']
 paths=[b.HERE/n for n in names+headers]
 fixture=b.OUT/'native-ui/screen-controls-view/f91f5a92987aef5bc52c';svg=b.OUT/'native-ui/screen-svg-deps/sdk/6.4.1/mingw_64'
 identity={p.name:b.sha(p) for p in paths};identity.update(compiler=b.sha(b.CXX),qt=b.sha(b.SDK/'bin/Qt6Quick.dll'),fixture={str(p.relative_to(fixture)):b.sha(p) for p in [fixture/'fixture.rcc',fixture/'extra.rcc',*(fixture/'private-imports').rglob('*')] if p.is_file()})
 out=b.OUT/'native-ui/screen-attachment'/hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()[:20];src=out/'private-source';src.mkdir(parents=True,exist_ok=False)
 for p in paths:(src/p.name).write_bytes(p.read_bytes())
 for n in ['fixture.rcc','extra.rcc']:(out/n).write_bytes((fixture/n).read_bytes())
 shutil.copytree(fixture/'private-imports',out/'private-imports')
 env=dict(os.environ,TEMP=str(out),TMP=str(out),PATH=str(svg/'bin')+os.pathsep+str(b.CXX.parent)+os.pathsep+str(b.SDK/'bin')+os.pathsep+os.environ['PATH'],QT_PLUGIN_PATH=str(svg/'plugins')+os.pathsep+str(b.SDK/'plugins'),QML2_IMPORT_PATH=str(out/'private-imports')+os.pathsep+str(b.SDK/'qml'),QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',QML_DISABLE_DISK_CACHE='1')
 modules=['Quick','Qml','QmlModels','Gui','Network','Core'];inc=['-I'+str(b.SDK/'include'),'-I'+str(src)]
 for m in modules:inc+=['-I'+str(b.SDK/'include'/('Qt'+m)),'-I'+str(b.SDK/'include'/('Qt'+m)/'6.4.1'),'-I'+str(b.SDK/'include'/('Qt'+m)/'6.4.1'/('Qt'+m))]
 cmd=[b.CXX,'-std=c++17','-O2','-fno-builtin','-DQT_NO_KEYWORDS','-DQT_NO_DEBUG',*inc,*[src/n for n in names],*[b.SDK/'lib'/('libQt6'+m+'.a') for m in modules],'-o',out/'test.exe']
 r=subprocess.run(list(map(str,cmd)),env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=120);(out/'build.log').write_text(r.stdout+r.stderr,encoding='utf8');assert not r.returncode,r.stdout+r.stderr
 r=subprocess.run([str(out/'test.exe'),str(out/'fixture.rcc'),str(out)],env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=60);(out/'run.log').write_text(r.stdout+r.stderr,encoding='utf8');assert not r.returncode,r.stdout+r.stderr
 assert all(b.sha(p)==identity[p.name] for p in paths)
 (out/'report.json').write_text(json.dumps(dict(identity=identity,passed=True,author_tests=True,independent_approval=False,stdout=r.stdout,camera_requests=0,qt='6.4.1 desktop',original_gui_attached=False,package_integrated=False,remaining='original global key forwarding and EVF-mode session attachment'),ensure_ascii=False,indent=2),encoding='utf8');print(out);print(r.stdout)
if __name__=='__main__':main()


