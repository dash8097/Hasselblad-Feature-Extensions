"""真实桌面 Qt 验证固定标量 slot 布局及回调生命周期，不代表相机 GUI。"""
from pathlib import Path
import hashlib,json,os,subprocess,ctypes
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[3]
BASE=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/native-ui'
SDK=BASE/'qt-sdk/6.4.1/mingw_64';CXX=BASE/'host-mingw/sdk/Tools/mingw1120_64/bin/g++.exe'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 ctypes.windll.kernel32.SetErrorMode(0x8003)
 sources={str(p.relative_to(HERE)):sha(p) for p in [HERE/'arm_bridge/slot_core.h',HERE/'test_native_slot.cpp',Path(__file__)]}
 identity={'sources':sources,'compiler':sha(CXX),'Qt6Core':sha(SDK/'bin/Qt6Core.dll')}
 revision=hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()[:20]
 out=BASE/'slot-test'/revision;out.mkdir(parents=True,exist_ok=True)
 env=dict(os.environ,PATH=str(CXX.parent)+os.pathsep+str(SDK/'bin')+os.pathsep+os.environ['PATH'],TEMP=str(out),TMP=str(out))
 binary=out/'native-slot-test.exe'
 args=[str(x) for x in [CXX,'-std=c++17','-O2','-Wall','-Wextra','-Werror','-DQT_NO_DEBUG','-DQT_CORE_LIB','-I'+str(SDK/'include'),'-I'+str(SDK/'include/QtCore'),HERE/'test_native_slot.cpp',SDK/'lib/libQt6Core.a','-o',binary]]
 p=subprocess.run(args,env=env,capture_output=True,text=True,timeout=120);assert not p.returncode,p.stdout+p.stderr
 p=subprocess.run([str(binary)],env=env,capture_output=True,text=True,timeout=10);(out/'console.txt').write_text(p.stdout+p.stderr);assert p.returncode==0,p.stdout+p.stderr
 assert 'NATIVE_SLOT_QT_PASS assertions=27' in p.stdout,p.stdout
 report={'identity':identity,'binary':sha(binary),'stdout':p.stdout,'exit':p.returncode,'camera_requests':0,'original_camera_qt_executed':False}
 (out/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(out);print(p.stdout)
if __name__=='__main__':main()
