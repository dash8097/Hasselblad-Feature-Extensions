"""原p69 QtQuick/Canvas vs 原生QPainter：无网络、无相机的实际离屏图像对照。"""
from pathlib import Path
import ctypes,hashlib,json,os,subprocess
import build_processing_tests as b
def replace_body(text,marker,body):
 start=text.index('{',text.index(marker));i=start+1;depth=1
 while depth:
  if text[i]=='{':depth+=1
  elif text[i]=='}':depth-=1
  i+=1
 return text[:start+1]+body+text[i-1:]
def main():
 assert Path.cwd().resolve()==b.ROOT
 ctypes.windll.kernel32.SetErrorMode(0x8003)
 base=b.OUT/'native-payload/revisions/daf41916a46d4978cd8d/private-payload/install'
 names=['animation_view.hpp','animation_view.cpp','animation_view_qt.hpp','animation_view_qt.cpp','shutter_animation_core.hpp','shutter_animation_core.cpp','test_animation_view.cpp','build_animation_view_tests.py']
 paths=[b.HERE/n for n in names]+[base/'p69-X2dShutterAnimation.qml',base/'p70-X2dShutterAnimationHost.qml']
 identity={p.name:b.sha(p) for p in paths};identity['compiler']=b.sha(b.CXX);identity['qt_gui']=b.sha(b.SDK/'bin/Qt6Gui.dll');identity['qt_quick']=b.sha(b.SDK/'bin/Qt6Quick.dll')
 fonts=[Path('C:/Windows/Fonts')/name for name in ('segoeui.ttf','seguisym.ttf','msyh.ttc')];identity['private_test_fonts']={p.name:b.sha(p) for p in fonts}
 rev=hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()[:20];out=b.OUT/'native-ui/animation-view'/rev;out.mkdir(parents=True,exist_ok=False)
 for p in paths:(out/p.name).write_bytes(p.read_bytes())
 (out/'private-fonts').mkdir()
 for p in fonts:(out/'private-fonts'/p.name).write_bytes(p.read_bytes())
 oracle=(base/'p69-X2dShutterAnimation.qml').read_text(encoding='utf8');oracle=replace_body(oracle,'function soundRequest(action)',' return; ');oracle=oracle.replace('property bool networkEnabled: true','property bool networkEnabled: false')
 (out/'oracle.qml').write_text(oracle,encoding='utf8')
 (out/'X2dEffectPreferences.qml').write_text('import QtQml\nQtObject { property bool networkEnabled: false; property bool ready: true; property int mode: 1 }\n',encoding='utf8')
 env=dict(os.environ,TEMP=str(out),TMP=str(out),PATH=str(b.CXX.parent)+os.pathsep+str(b.SDK/'bin')+os.pathsep+os.environ['PATH'],QML_DISABLE_DISK_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',QSG_RENDER_LOOP='basic',QML2_IMPORT_PATH=str(b.SDK/'qml'))
 cmd=[b.CXX,'-std=c++17','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-DQT_NO_DEBUG','-I'+str(b.SDK/'include')]
 for mod in ('QtCore','QtGui','QtQml','QtQuick'):cmd+=['-I'+str(b.SDK/'include'/mod)]
 cmd +=[out/n for n in ('animation_view.cpp','animation_view_qt.cpp','shutter_animation_core.cpp','test_animation_view.cpp')]
 cmd +=[b.SDK/'lib'/('lib'+mod+'.a') for mod in ('Qt6Quick','Qt6Qml','Qt6Gui','Qt6Core')]+['-o',out/'test.exe']
 r=subprocess.run(list(map(str,cmd)),env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=120);(out/'build.log').write_text(r.stdout+r.stderr,encoding='utf8');assert not r.returncode,r.stdout+r.stderr
 r=subprocess.run([str(out/'test.exe'),str(out/'oracle.qml'),str(out)],env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=90);(out/'run.log').write_text(r.stdout+r.stderr,encoding='utf8');assert not r.returncode,r.stdout+r.stderr
 assert all(b.sha(p)==identity[p.name] for p in paths)
 (out/'report.json').write_text(json.dumps(dict(identity=identity,passed=True,author_tests=True,independent_approval=False,binary=b.sha(out/'test.exe'),oracle_adapted_sha256=b.sha(out/'oracle.qml'),stdout=r.stdout,camera_requests=0,qt='6.4.1 offline software raster',arm_qt_profile_bound=False,package_integrated=False),ensure_ascii=False,indent=2),encoding='utf8')
 print(out);print(r.stdout)
if __name__=='__main__':main()
