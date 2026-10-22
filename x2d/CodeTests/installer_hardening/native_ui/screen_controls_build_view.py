"""原厂TextValueRow/MenuBoolSelector私有资源与Qt6.4.1实际动态控件测试。"""
from pathlib import Path
import ctypes,hashlib,json,os,subprocess,shutil,sys
sys.dont_write_bytecode=True
import build_processing_tests as b
def main():
 assert Path.cwd().resolve()==b.ROOT
 ctypes.windll.kernel32.SetErrorMode(0x8003)
 names=['screen_notice_view.cpp','screen_notice_view.hpp','screen_controls_view.cpp','screen_controls_view.hpp','screen_controls.cpp','screen_controls.hpp','screen_policy.cpp','screen_policy.hpp','stock_factory.cpp','stock_factory.hpp','abi_view_api.cpp','abi_view_api.hpp','qt_stock_profile.cpp','qt_stock_profile.hpp','qt_abi_profile.cpp','qt_abi_profile.hpp','qt_menu_profile.cpp','qt_menu_profile.hpp','menu_discovery.cpp','menu_discovery.hpp','effect_view_core.hpp','local_heap.hpp','screen_controls_view_test.cpp','screen_controls_build_view.py']
 paths=[b.HERE/n for n in names];fixture=b.OUT/'checker-ui/settings-view-fourth';rcc=fixture/'fixture.rcc'
 identity={p.name:b.sha(p) for p in paths};identity.update(compiler=b.sha(b.CXX),qt=b.sha(b.SDK/'bin/Qt6Quick.dll'),rcc=b.sha(rcc));sys.path.insert(0,str(b.ROOT/'x2d/CodeTests/temporary_af_speed_probe'));from inspect_menu_resources import load_gui,resources
 raw=dict(resources(load_gui()));extra={k:v for k,v in raw.items() if k in (':/app/qml/popups/PopupIconText.qml',':/app/qml/components/WritingIndicator.qml',':/app/qml/mainmenu/MenuBoolSelector.qml',':/app/qml/components/ToggleSwitch.qml',':/app/qml/components/TextCheckbox.qml',':/app/qml/components/Checkbox.qml') or k.startswith(':/icons/ic_menu_switch') or k.startswith(':/icons/checkbox_') or k.startswith(':/icons/infoDialogIcon') or k.startswith(':/icons/ic_controlscreen_writing')}
 identity['extra_original_resources']={k:hashlib.sha256(v).hexdigest() for k,v in extra.items()}
 svg=b.OUT/'native-ui/screen-svg-deps/sdk/6.4.1/mingw_64';identity['svg_plugin']=b.sha(svg/'plugins/imageformats/qsvg.dll');identity['qt_svg']=b.sha(svg/'bin/Qt6Svg.dll')
 identity['test_font']=b.sha(Path('C:/Windows/Fonts/msyh.ttc'));identity['fixture_imports']={str(p.relative_to(fixture/'private-fixture/imports')):b.sha(p) for p in (fixture/'private-fixture/imports').rglob('*') if p.is_file()}
 out=b.OUT/'native-ui/screen-controls-view'/hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()[:20];src=out/'private-source';src.mkdir(parents=True,exist_ok=False)
 for p in paths:(src/p.name).write_bytes(p.read_bytes())
 (out/'private-font.ttc').write_bytes(Path('C:/Windows/Fonts/msyh.ttc').read_bytes());(out/'fixture.rcc').write_bytes(rcc.read_bytes());shutil.copytree(fixture/'private-fixture/imports',out/'private-imports')
 entries=[]
 for i,(k,v) in enumerate(extra.items()):
  dst=out/('original-%d'%i);dst.write_bytes(v);entries.append('<file alias="'+k[2:]+'">'+dst.as_posix()+'</file>')
 (out/'extra.qrc').write_text('<RCC><qresource prefix="/">'+''.join(entries)+'</qresource></RCC>',encoding='utf8')
 const=out/'private-imports/com/hasselblad/constants/Constants.qml';text=const.read_text(encoding='utf8');const.write_text(text[:text.rfind('}')]+ ';property string settingsMenuSettingSwitchFontName:"Microsoft YaHei";property int settingsMenuSettingSwitchFontSize:28;property color menuItemColor:"white";property int defaultCheckBoxSize:32;property real infoPopupInternalTopMargin:30;property real infoPopupInternalBottomMargin:30;property real infoPopupTextIconSpacing:20;property real infoPopupMinHeight:220;property real infoPopupWidth:500;property color popupBorderColor:"gray";property int popupBorderWidth:1;property real infoPopupTextMaxWidth:430;property color cameraViewNormalTextColor:"white";property int confirmDialogInfoFontSize:28}',encoding='utf8')
 env=dict(os.environ,TEMP=str(out),TMP=str(out),PATH=str(svg/'bin')+os.pathsep+str(b.CXX.parent)+os.pathsep+str(b.SDK/'bin')+os.pathsep+os.environ['PATH'],QT_PLUGIN_PATH=str(svg/'plugins')+os.pathsep+str(b.SDK/'plugins'),QML2_IMPORT_PATH=str(out/'private-imports')+os.pathsep+str(b.SDK/'qml'),QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',QML_DISABLE_DISK_CACHE='1')
 r=subprocess.run([str(b.SDK/'bin/rcc.exe'),'--binary',str(out/'extra.qrc'),'-o',str(out/'extra.rcc')],env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=30);assert not r.returncode,r.stderr
 modules=['Quick','Qml','QmlModels','Gui','Network','Core'];inc=['-I'+str(b.SDK/'include'),'-I'+str(src)]
 for m in modules:inc+=['-I'+str(b.SDK/'include'/('Qt'+m)),'-I'+str(b.SDK/'include'/('Qt'+m)/'6.4.1'),'-I'+str(b.SDK/'include'/('Qt'+m)/'6.4.1'/('Qt'+m))]
 cmd=[b.CXX,'-std=c++17','-O2','-fno-builtin','-DQT_NO_KEYWORDS','-DQT_NO_DEBUG',*inc,*[src/n for n in names if n.endswith('.cpp')],*[b.SDK/'lib'/('libQt6'+m+'.a') for m in modules],'-Wl,--wrap=malloc','-o',out/'test.exe']
 r=subprocess.run(list(map(str,cmd)),env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=120);(out/'build.log').write_text(r.stdout+r.stderr,encoding='utf8');assert not r.returncode,r.stdout+r.stderr
 r=subprocess.run([str(out/'test.exe'),str(out/'fixture.rcc'),str(out)],env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=90);(out/'run.log').write_text(r.stdout+r.stderr,encoding='utf8');assert not r.returncode,r.stdout+r.stderr
 assert all(b.sha(p)==identity[p.name] for p in paths)
 (out/'report.json').write_text(json.dumps(dict(identity=identity,passed=True,author_tests=True,independent_approval=False,stdout=r.stdout,camera_requests=0,qt='6.4.1 desktop',stock_component='original TextValueRow/MenuBoolSelector/ToggleSwitch/TextCheckbox/Checkbox/PopupIconText/WritingIndicator and SVG resources',standins='Constants/KeyFn/UiImage plus original-SVG QImageProvider',original_gui_attached=False,package_integrated=False),ensure_ascii=False,indent=2),encoding='utf8');print(out);print(r.stdout)
if __name__=='__main__':main()
