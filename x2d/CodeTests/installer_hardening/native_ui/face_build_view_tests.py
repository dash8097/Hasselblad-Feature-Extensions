"""原p14/p15可视片段与C++列表的Qt6.4.1实际离屏对照；不运行相机代理。"""
from pathlib import Path
import ctypes,hashlib,json,os,subprocess
import build_processing_tests as b
def main():
 assert Path.cwd().resolve()==b.ROOT
 ctypes.windll.kernel32.SetErrorMode(0x8003)
 base=b.OUT/'native-payload/revisions/5cb27e29bb84fe40f8b3/private-payload/install'
 names=['face_view.hpp','face_view.cpp','face_view_qt.hpp','face_view_qt.cpp','face_model.hpp','face_model.cpp','face_test_view.cpp','face_build_view_tests.py','protocol_status.hpp','protocol_status.cpp','protocol_text.hpp','selection_policies.hpp']
 paths=[b.HERE/n for n in names]+[base/'p14-X2dFacePointLayer.qml',base/'p15-X2dEyeOverlay.qml']
 fonts=[Path('C:/Windows/Fonts/msyh.ttc')];identity={p.name:b.sha(p) for p in paths};identity.update(compiler=b.sha(b.CXX),qt=b.sha(b.SDK/'bin/Qt6Quick.dll'),font=b.sha(fonts[0]))
 out=b.OUT/'native-ui/face-view'/hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()[:20];out.mkdir(parents=True,exist_ok=False)
 for p in paths:(out/p.name).write_bytes(p.read_bytes())
 (out/'font.ttc').write_bytes(fonts[0].read_bytes())
 source=(base/'p14-X2dFacePointLayer.qml').read_text(encoding='utf8');visual=source[source.index('    Text {'):source.index('    Connections {',source.index('    Text {'))]
 visual=visual.replace('HblmTypes.E_AfRoiType_Face','1')
 prefix="""import QtQuick
Item { id:root
 property bool faceMode:true; property bool ordinaryAfFallback:false; property bool personLocked:false
 property rect lockedRegion:Qt.rect(0,0,0,0); property bool selectorOnly:false; property bool pointPending:false
 property real pointX:.5; property real pointY:.5; property var candidateFaces:[]; property bool actualFaceValid:false
 property int selectedId:-1; property bool eyeTargetVisible:false; property int foundFocusState:3
 property var vm:({canShowAF:false,faceDetectionPaused:false,focusRegion:Qt.rect(0,0,0,0),detectedFace:{faceId:-1},afRoiType:0,afIndicatorStatus:0})
 visible:faceMode
"""
 (out/'oracle.qml').write_text(prefix+visual+'FaceEyeOracle {objectName:"eyeMain";anchors.fill:parent}\nFaceEyeOracle {objectName:"eyeFallback";anchors.fill:parent}\n}\n',encoding='utf8')
 src=(base/'p15-X2dEyeOverlay.qml').read_text(encoding='utf8');eye=src[src.index('    Repeater {'):src.rfind('}')].replace('HblmTypes.E_AfRoiType_EyeLeft','2').replace('HblmTypes.E_AfRoiType_EyeRight','3')
 (out/'FaceEyeOracle.qml').write_text('import QtQuick\nItem {id:root; clip:true; property var face:null; property bool focusConfirmed:false; property int afRoiType:0; property bool personLocked:false; property int activeEye:-1; property int preferredEye:-1;\n'+eye+'}\n',encoding='utf8')
 env=dict(os.environ,TEMP=str(out),TMP=str(out),PATH=str(b.CXX.parent)+os.pathsep+str(b.SDK/'bin')+os.pathsep+os.environ['PATH'],QML_DISABLE_DISK_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',QSG_RENDER_LOOP='basic',QML2_IMPORT_PATH=str(b.SDK/'qml'))
 cmd=[b.CXX,'-std=c++17','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-DQT_NO_DEBUG','-I'+str(b.SDK/'include')]
 for mod in ('QtCore','QtGui','QtQml','QtQuick'):cmd+=['-I'+str(b.SDK/'include'/mod)]
 cmd +=[out/n for n in ('face_view.cpp','face_view_qt.cpp','face_model.cpp','protocol_status.cpp','face_test_view.cpp')]
 cmd +=[b.SDK/'lib'/('lib'+mod+'.a') for mod in ('Qt6Quick','Qt6Qml','Qt6Gui','Qt6Core')]+['-o',out/'test.exe']
 r=subprocess.run(list(map(str,cmd)),env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=120);(out/'build.log').write_text(r.stdout+r.stderr,encoding='utf8');assert not r.returncode,r.stdout+r.stderr
 r=subprocess.run([str(out/'test.exe'),str(out)],env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=90);(out/'run.log').write_text(r.stdout+r.stderr,encoding='utf8');assert not r.returncode,r.stdout+r.stderr
 assert all(b.sha(p)==identity[p.name] for p in paths)
 (out/'report.json').write_text(json.dumps(dict(identity=identity,passed=True,author_tests=True,independent_approval=False,stdout=r.stdout,camera_requests=0,qt='6.4.1 offscreen software',original_visual_fragments=True,original_full_component_executed=False,arm_qt_binding=False,package_integrated=False),ensure_ascii=False,indent=2),encoding='utf8');print(out);print(r.stdout)
if __name__=='__main__':main()
