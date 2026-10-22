"""真实 Qt 原生设置行和触摸事件离线试验，不连接相机。"""
from pathlib import Path
import hashlib,json,os,subprocess,ctypes,sys
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[3]
BASE=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/native-ui';SDK=BASE/'qt-sdk/6.4.1/mingw_64';CXX=BASE/'host-mingw/sdk/Tools/mingw1120_64/bin/g++.exe'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 ctypes.windll.kernel32.SetErrorMode(0x8003)
 names=['effect_view_core.hpp','abi_view_api.hpp','local_heap.hpp','menu_discovery.hpp','stock_factory.hpp','stock_factory.cpp','qt_stock_profile.hpp','qt_stock_profile.cpp','effect_view_core.cpp','abi_view_api.cpp','qt_abi_profile.hpp','qt_abi_profile.cpp','processing_presentation.hpp','processing_presentation.cpp','test_original_stock.cpp','build_original_stock.py']
 identity={'sources':{n:sha(HERE/n) for n in names},'compiler':sha(CXX),'qtquick':sha(SDK/'bin/Qt6Quick.dll')}
 rev=hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()[:20];out=BASE/'original-stock'/rev;out.mkdir(parents=True,exist_ok=True)
 env=dict(os.environ,PATH=str(CXX.parent)+os.pathsep+str(SDK/'bin')+os.pathsep+os.environ['PATH'],TEMP=str(out),TMP=str(out),QML2_IMPORT_PATH=str(SDK/'qml'),QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software')
 include=['-I'+str(SDK/'include')];modules=['Quick','Qml','QmlModels','Gui','Network','Core','Test']
 for m in modules:
  include+=['-I'+str(SDK/'include'/('Qt'+m)),'-I'+str(SDK/'include'/('Qt'+m)/'6.4.1'),'-I'+str(SDK/'include'/('Qt'+m)/'6.4.1'/('Qt'+m))]
 sys.path.insert(0,str(HERE.parent.parent/'temporary_af_speed_probe'));sys.path.insert(0,str(HERE.parent.parent/'pixel_shift_rgb'))
 from inspect_menu_resources import load_gui,resources
 from processing_ui_fixture import prepare
 fixture=out/'private-fixture';fixture.mkdir(exist_ok=True)
 def write(n,text):
  p=fixture/n;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(text,encoding='utf-8')
 prepare(write,dict(resources(load_gui())))
 def singleton(module,name,body):
  write('imports/com/hasselblad/'+module+'/qmldir','module com.hasselblad.'+module+'\nsingleton '+name+' 1.0 '+name+'.qml\n')
  write('imports/com/hasselblad/'+module+'/'+name+'.qml','pragma Singleton\nimport QtQuick\nQtObject { '+body+' }')
 singleton('constants','Constants','property real scaleFactor:1;property real scaleFactorX:1;property real scaleFactorY:1;property real cameraViewRightMargin:20;property real cameraViewMarginY:20;property color menuHeaderColor:"white";property color colorWhite:"white";property color colorBlack:"black";property color menuBackgroundColor:"#202020";property string progressFragmentShaderName:""')
 singleton('types','HblmTypes','enum Value { E_CameraKeyOption_Override=1 }')
 singleton('keys','KeyFn','enum Function { Escape, Abort, JstkEscape, Select, PopAccept }\nfunction isFn(key,modifiers,fn){return false}\nfunction isUnblockableCustomButton(key,modifiers){return false}')
 env['QML2_IMPORT_PATH']=str(fixture/'imports')+os.pathsep+str(SDK/'qml')
 files=sorted((fixture/'factory').rglob('*'));entries=['<file alias="'+p.relative_to(fixture/'factory').as_posix()+'">'+p.as_posix()+'</file>' for p in files if p.is_file()]
 (out/'fixture.qrc').write_text('<RCC><qresource prefix="/app/qml">'+''.join(entries)+'</qresource></RCC>',encoding='utf-8')
 resource=out/'fixture.rcc'
 r=subprocess.run([str(SDK/'bin/rcc.exe'),'--binary',str(out/'fixture.qrc'),'-o',str(resource)],env=env,capture_output=True,text=True,timeout=30);assert r.returncode==0,r.stderr
 binary=out/'native-original-stock.exe'
 args=[str(x) for x in [CXX,'-std=c++17','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-DQT_NO_DEBUG','-DQT_GUI_LIB','-DQT_CORE_LIB',*include,*[HERE/n for n in names if n.endswith('.cpp')],*[SDK/'lib'/('libQt6'+m+'.a') for m in modules],'-o',binary]]
 p=subprocess.run(args,env=env,capture_output=True,text=True,timeout=120);(out/'build.log').write_text(p.stdout+p.stderr);assert not p.returncode,p.stdout+p.stderr
 p=subprocess.run([str(binary),str(resource)],env=env,capture_output=True,text=True,timeout=20);(out/'console.txt').write_text(p.stdout+p.stderr);assert p.returncode==0,p.stdout+p.stderr
 assert 'NATIVE_ORIGINAL_STOCK_PASS' in p.stdout,p.stdout
 report={'identity':identity,'binary':sha(binary),'stdout':p.stdout,'exit':p.returncode,'camera_requests':0,'business_qml':False,'original_camera_qt_executed':False,'transport':'explicit recorder, no HTTP'}
 (out/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(out);print(p.stdout)
if __name__=='__main__':main()
