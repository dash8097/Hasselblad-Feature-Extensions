"""5cb冻结自有人脸业务模型作者回归，不连接Camera/System代理。"""
from pathlib import Path
import ctypes,hashlib,json,os,subprocess
import build_processing_tests as b
def main():
 assert Path.cwd().resolve()==b.ROOT
 ctypes.windll.kernel32.SetErrorMode(0x8003)
 base=b.OUT/'native-payload/revisions/5cb27e29bb84fe40f8b3/private-payload/install'
 names=['face_model.hpp','face_model.cpp','face_test_model.cpp','face_test_session.cpp','face_session.hpp','face_session.cpp','face_io.h','face_build_tests.py','protocol_status.hpp','protocol_status.cpp','protocol_text.hpp','selection_policies.hpp']
 oracle=['p10-X2dFaceModeController.qml','p11-X2dFaceModePopup.qml','p12-X2dEyeAfController.qml','p14-X2dFacePointLayer.qml','p15-X2dEyeOverlay.qml','p13-EyePreferencePolicy.js','p16-NearestFacePolicy.js']
 paths=[b.HERE/n for n in names]+[base/n for n in oracle];identity={p.name:b.sha(p) for p in paths};identity['compiler']=b.sha(b.CXX);identity['qt']=b.sha(b.SDK/'bin/Qt6Qml.dll')
 rev=hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()[:20];out=b.OUT/'native-ui/face-model'/rev;out.mkdir(parents=True,exist_ok=False)
 for p in paths:(out/p.name).write_bytes(p.read_bytes())
 env=dict(os.environ,TEMP=str(out),TMP=str(out),PATH=str(b.CXX.parent)+os.pathsep+str(b.SDK/'bin')+os.pathsep+os.environ['PATH'],QML_DISABLE_DISK_CACHE='1')
 cmd=[b.CXX,'-std=c++17','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-DQT_NO_DEBUG','-I'+str(b.SDK/'include'),'-I'+str(b.SDK/'include/QtCore'),'-I'+str(b.SDK/'include/QtQml'),out/'face_model.cpp',out/'protocol_status.cpp',out/'face_test_model.cpp',b.SDK/'lib/libQt6Qml.a',b.SDK/'lib/libQt6Core.a','-o',out/'test.exe']
 r=subprocess.run(list(map(str,cmd)),env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=120);(out/'build.log').write_text(r.stdout+r.stderr,encoding='utf8');assert not r.returncode,r.stdout+r.stderr
 r=subprocess.run([str(out/'test.exe'),str(out/oracle[0])],env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=60);(out/'run.log').write_text(r.stdout+r.stderr,encoding='utf8');assert not r.returncode,r.stdout+r.stderr
 model_stdout=r.stdout
 session_cmd=[v for v in cmd if v!=out/'face_test_model.cpp']
 session_cmd[session_cmd.index('-o')+1]=out/'session.exe'
 session_cmd[session_cmd.index('-o'):session_cmd.index('-o')]=[out/'face_session.cpp',out/'face_test_session.cpp']
 r=subprocess.run(list(map(str,session_cmd)),env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=120);(out/'session-build.log').write_text(r.stdout+r.stderr,encoding='utf8');assert not r.returncode,r.stdout+r.stderr
 r=subprocess.run([str(out/'session.exe')],env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=60);(out/'session-run.log').write_text(r.stdout+r.stderr,encoding='utf8');assert not r.returncode,r.stdout+r.stderr
 r.stdout=model_stdout+r.stdout
 assert all(b.sha(p)==identity[p.name] for p in paths)
 (out/'report.json').write_text(json.dumps(dict(identity=identity,passed=True,author_tests=True,independent_approval=False,stdout=r.stdout,camera_requests=0,package_integrated=False),ensure_ascii=False,indent=2),encoding='utf8');print(out);print(r.stdout)
if __name__=='__main__':main()
