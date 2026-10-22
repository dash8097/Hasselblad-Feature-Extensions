"""原生控制器作者测试：固定 p21 差分、异步协议/边界；不连接设备。"""
from pathlib import Path
import ctypes,hashlib,json,os,subprocess
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
OUT=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7'
SDK=OUT/'native-ui/qt-sdk/6.4.1/mingw_64'
CXX=OUT/'native-ui/host-mingw/sdk/Tools/mingw1120_64/bin/g++.exe'
ORACLE=OUT/'native-payload/revisions/daf41916a46d4978cd8d/private-payload/install/p21-PixelOverclockController.qml'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 assert Path.cwd().resolve()==ROOT
 ctypes.windll.kernel32.SetErrorMode(0x8003)
 paths=[HERE/n for n in ('pixel_controller.hpp','pixel_controller.cpp','protocol_status.hpp','protocol_status.cpp','protocol_text.hpp','processing_presentation.hpp','test_pixel_controller.cpp','build_pixel_controller_tests.py')]+[ORACLE]
 identity={p.name:sha(p) for p in paths};identity['compiler']=sha(CXX);identity['qt']=sha(SDK/'bin/Qt6Qml.dll')
 rev=hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()[:20]
 out=OUT/'native-ui/pixel-controller'/rev;out.mkdir(parents=True,exist_ok=False)
 for p in paths:(out/p.name).write_bytes(p.read_bytes())
 env=dict(os.environ,TEMP=str(out),TMP=str(out),PATH=str(CXX.parent)+os.pathsep+str(SDK/'bin')+os.pathsep+os.environ['PATH'],QML_DISABLE_DISK_CACHE='1')
 cmd=[CXX,'-std=c++17','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-DQT_NO_DEBUG','-DQT_CORE_LIB','-DQT_QML_LIB','-I'+str(SDK/'include'),'-I'+str(SDK/'include/QtCore'),'-I'+str(SDK/'include/QtQml'),out/'pixel_controller.cpp',out/'protocol_status.cpp',out/'test_pixel_controller.cpp',SDK/'lib/libQt6Qml.a',SDK/'lib/libQt6Core.a','-o',out/'test.exe']
 cmd.insert(1,'-Wl,--wrap=realloc');cmd.insert(1,'-fno-builtin')
 p=subprocess.run(list(map(str,cmd)),env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=120);(out/'build.log').write_text(p.stdout+p.stderr,encoding='utf8');assert not p.returncode,p.stdout+p.stderr
 p=subprocess.run([str(out/'test.exe'),str(out/ORACLE.name)],env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=60);(out/'run.log').write_text(p.stdout+p.stderr,encoding='utf8');assert not p.returncode,p.stdout+p.stderr
 assert all(sha(p)==identity[p.name] for p in paths)
 (out/'report.json').write_text(json.dumps(dict(identity=identity,passed=True,author_tests=True,independent_approval=False,binary=sha(out/'test.exe'),stdout=p.stdout,camera_requests=0,package_integrated=False),ensure_ascii=False,indent=2),encoding='utf8')
 print(out);print(p.stdout)
if __name__=='__main__':main()
