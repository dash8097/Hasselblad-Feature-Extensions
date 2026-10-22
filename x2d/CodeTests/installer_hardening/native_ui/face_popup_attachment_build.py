"""原厂 Popover/FramedImage 私有资源和真实 Qt，离线验证自有 C++ 弹窗。"""
from pathlib import Path
import ctypes,hashlib,json,os,subprocess,shutil,sys
sys.dont_write_bytecode=True
import build_processing_tests as b
def main():
 assert Path.cwd().resolve()==b.ROOT
 ctypes.windll.kernel32.SetErrorMode(0x8003)
 names=['qt_key_profile.cpp','qt_key_profile.hpp','face_popup_attachment.cpp', 'face_popup_attachment.hpp', 'face_factory.cpp', 'face_factory.hpp', 'face_generate_factory.py', 'face_popup_view.cpp', 'face_popup_view.hpp', 'face_popup_profile.py', 'face_model.cpp', 'face_model.hpp', 'protocol_status.cpp', 'protocol_status.hpp', 'protocol_text.hpp', 'selection_policies.hpp', 'signal_bridge.cpp', 'signal_bridge.hpp', 'qt_signal_profile.cpp', 'qt_signal_profile.hpp', 'stock_factory.cpp', 'stock_factory.hpp', 'abi_view_api.cpp', 'abi_view_api.hpp', 'qt_stock_profile.cpp', 'qt_stock_profile.hpp', 'qt_abi_profile.cpp', 'qt_abi_profile.hpp', 'qt_menu_profile.cpp', 'qt_menu_profile.hpp', 'menu_discovery.cpp', 'menu_discovery.hpp', 'effect_view_core.hpp', 'local_heap.hpp', 'face_popup_attachment_test.cpp', 'face_popup_attachment_build.py', 'factory_proxy.hpp', 'binding_lease.hpp', 'binding_lease.cpp', 'qt_binding_profile.hpp', 'qt_binding_profile.cpp', 'screen_key_profile.hpp', 'screen_key_profile.cpp', 'key_filter.hpp', 'key_mapping.hpp']
 paths=[b.HERE/n for n in names];fixture=b.OUT/'native-ui/screen-controls-view/f91f5a92987aef5bc52c'
 sys.path.insert(0,str(b.ROOT/'x2d/CodeTests/temporary_af_speed_probe'));from inspect_menu_resources import load_gui,resources
 raw=dict(resources(load_gui()));required=[':/app/qml/popups/Popover.qml',':/app/qml/components/buttons/FramedImage.qml',':/app/qml/components/buttons/FramedItem.qml',':/app/qml/components/ScaledImage.qml',':/app/qml/scripts/Keys.js']
 extra={k:v for k,v in raw.items() if k in required or k.startswith(':/icons/ic_face_detection_popup_') or k.startswith(':/icons/ic_no_face_detection_popup')}
 assert all(k in extra for k in required)
 identity={p.name:b.sha(p) for p in paths};identity.update(compiler=b.sha(b.CXX),qt=b.sha(b.SDK/'bin/Qt6Quick.dll'),fixture=b.sha(fixture/'fixture.rcc'),resources={k:hashlib.sha256(v).hexdigest() for k,v in extra.items()},imports={str(p.relative_to(fixture/'private-imports')):b.sha(p) for p in (fixture/'private-imports').rglob('*') if p.is_file()})
 out=b.OUT/'native-ui/face-popup-attachment'/hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()[:20];src=out/'private-source';src.mkdir(parents=True,exist_ok=False)
 for p in paths:(src/p.name).write_bytes(p.read_bytes())
 (out/'private-font.ttc').write_bytes(Path('C:/Windows/Fonts/msyh.ttc').read_bytes());(out/'fixture.rcc').write_bytes((fixture/'fixture.rcc').read_bytes());shutil.copytree(fixture/'private-imports',out/'private-imports')
 const=out/'private-imports/com/hasselblad/constants/Constants.qml';text=const.read_text(encoding='utf8');const.write_text(text[:text.rfind('}')]+ ';property real scaleFactorY:1;property color popupFadeoutColor:"black";property color popupBackgroundColor:"#101010";property real popupHeaderHeight:64;property real popupHeaderTextSize:32;property color popupTextColor:"white";property real fadeOutOpacity:0.7;property int fadeOutDuration:10}',encoding='utf8')
 types=out/'private-imports/com/hasselblad/types';types.mkdir(parents=True,exist_ok=True);(types/'qmldir').write_text('module com.hasselblad.types\nsingleton HblmTypes 1.0 HblmTypes.qml\n',encoding='utf8');(types/'HblmTypes.qml').write_text('pragma Singleton\nimport QtQuick\nQtObject {}\n',encoding='utf8')
 entries=[]
 for i,(k,v) in enumerate(extra.items()):
  dst=out/('original-%d'%i);dst.write_bytes(v);entries.append('<file alias="'+k[2:]+'">'+dst.as_posix()+'</file>')
 (out/'extra.qrc').write_text('<RCC><qresource prefix="/">'+''.join(entries)+'</qresource></RCC>',encoding='utf8')
 svg=b.OUT/'native-ui/screen-svg-deps/sdk/6.4.1/mingw_64';env=dict(os.environ,TEMP=str(out),TMP=str(out),PATH=str(svg/'bin')+os.pathsep+str(b.CXX.parent)+os.pathsep+str(b.SDK/'bin')+os.pathsep+os.environ['PATH'],QT_PLUGIN_PATH=str(svg/'plugins')+os.pathsep+str(b.SDK/'plugins'),QML2_IMPORT_PATH=str(out/'private-imports')+os.pathsep+str(b.SDK/'qml'),QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',QML_DISABLE_DISK_CACHE='1')
 r=subprocess.run([str(b.SDK/'bin/rcc.exe'),'--binary',str(out/'extra.qrc'),'-o',str(out/'extra.rcc')],env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=30);assert not r.returncode,r.stderr
 modules=['Test','Quick','Qml','QmlModels','Gui','Network','Core'];inc=['-I'+str(b.SDK/'include'),'-I'+str(src)]
 for m in modules:inc+=['-I'+str(b.SDK/'include'/('Qt'+m)),'-I'+str(b.SDK/'include'/('Qt'+m)/'6.4.1'),'-I'+str(b.SDK/'include'/('Qt'+m)/'6.4.1'/('Qt'+m))]
 cmd=[b.CXX,'-std=c++17','-O2','-fno-builtin','-DQT_GUI_LIB','-DQT_NO_KEYWORDS','-DQT_NO_DEBUG',*inc,*[src/n for n in names if n.endswith('.cpp')],*[b.SDK/'lib'/('libQt6'+m+'.a') for m in modules],'-o',out/'test.exe']
 r=subprocess.run(list(map(str,cmd)),env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=120);(out/'build.log').write_text(r.stdout+r.stderr,encoding='utf8');assert not r.returncode,r.stdout+r.stderr
 r=subprocess.run([str(out/'test.exe'),str(out/'fixture.rcc'),str(out)],env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=90);(out/'run.log').write_text(r.stdout+r.stderr,encoding='utf8');assert not r.returncode,r.stdout+r.stderr
 assert all(b.sha(p)==identity[p.name] for p in paths)
 (out/'report.json').write_text(json.dumps(dict(identity=identity,passed=True,author_tests=True,independent_approval=False,stdout=r.stdout,camera_requests=0,qt='6.4.1 desktop',standins='Constants/KeyFn/UiImage/HblmTypes and original SVG provider',original_gui_attached=False,package_integrated=False),ensure_ascii=False,indent=2),encoding='utf8');print(out);print(r.stdout)
if __name__=='__main__':main()
