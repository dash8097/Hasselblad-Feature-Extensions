"""真实 Qt 原生设置行和触摸事件离线试验，不连接相机。"""
from pathlib import Path
import hashlib,json,os,subprocess,ctypes
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[3]
BASE=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/native-ui';SDK=BASE/'qt-sdk/6.4.1/mingw_64';CXX=BASE/'host-mingw/sdk/Tools/mingw1120_64/bin/g++.exe'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 ctypes.windll.kernel32.SetErrorMode(0x8003)
 names=['effect_view_core.hpp','effect_view_core.cpp','local_heap.hpp','qt_view_api.hpp','qt_view_api.cpp','abi_view_api.hpp','abi_view_api.cpp','qt_abi_profile.hpp','qt_abi_profile.cpp','test_effect_view.cpp','build_effect_view.py']
 identity={'sources':{n:sha(HERE/n) for n in names},'compiler':sha(CXX),'qtquick':sha(SDK/'bin/Qt6Quick.dll')}
 rev=hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()[:20];out=BASE/'effect-view'/rev;out.mkdir(parents=True,exist_ok=True)
 env=dict(os.environ,PATH=str(CXX.parent)+os.pathsep+str(SDK/'bin')+os.pathsep+os.environ['PATH'],TEMP=str(out),TMP=str(out),QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software')
 include=['-I'+str(SDK/'include')];modules=['Quick','Qml','QmlModels','Gui','Network','Core','Test']
 for m in modules:
  include+=['-I'+str(SDK/'include'/('Qt'+m)),'-I'+str(SDK/'include'/('Qt'+m)/'6.4.1'),'-I'+str(SDK/'include'/('Qt'+m)/'6.4.1'/('Qt'+m))]
 binary=out/'native-effect-view.exe'
 args=[str(x) for x in [CXX,'-std=c++17','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-DQT_NO_DEBUG','-DQT_GUI_LIB','-DQT_CORE_LIB',*include,*[HERE/n for n in names if n.endswith('.cpp')],*[SDK/'lib'/('libQt6'+m+'.a') for m in modules],'-o',binary]]
 p=subprocess.run(args,env=env,capture_output=True,text=True,timeout=120);(out/'build.log').write_text(p.stdout+p.stderr);assert not p.returncode,p.stdout+p.stderr
 p=subprocess.run([str(binary),str(out/'native-effect-view.png')],env=env,capture_output=True,text=True,timeout=20);(out/'console.txt').write_text(p.stdout+p.stderr);assert p.returncode==0,p.stdout+p.stderr
 assert 'NATIVE_EFFECT_VIEW_PASS' in p.stdout,p.stdout
 abi=subprocess.run([str(binary),str(out/'abi-effect-view.png'),'--abi'],env=env,capture_output=True,text=True,timeout=20);(out/'abi-console.txt').write_text(abi.stdout+abi.stderr);assert abi.returncode==0,abi.stdout+abi.stderr
 report={'identity':identity,'binary':sha(binary),'stdout':p.stdout,'abi_stdout':abi.stdout,'exit':p.returncode,'camera_requests':0,'business_qml':False,'original_camera_qt_executed':False,'transport':'explicit recorder, no HTTP'}
 (out/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(out);print(p.stdout)
if __name__=='__main__':main()
