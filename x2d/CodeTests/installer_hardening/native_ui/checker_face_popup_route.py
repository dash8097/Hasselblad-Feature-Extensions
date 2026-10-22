from pathlib import Path
import sys,os,subprocess,hashlib,json,shutil,ctypes
sys.dont_write_bytecode=True
import build_processing_tests as b
base=b.OUT/'native-ui/face-popup-attachment'/ (sys.argv[1] if len(sys.argv)>1 else 'cc0656ca16e8d13c9de9')
ids={'checker':b.sha(Path(__file__)),'base':b.sha(base/'report.json')};out=b.OUT/'checker-ui/face-popup-routes'/hashlib.sha256(json.dumps(ids,sort_keys=True).encode()).hexdigest()[:20];src=out/'private-source';shutil.copytree(base/'private-source',src)
for n in ('fixture.rcc','extra.rcc','private-font.ttc'):(out/n).write_bytes((base/n).read_bytes())
shutil.copytree(base/'private-imports',out/'private-imports')
p=src/'face_popup_attachment_test.cpp';t=p.read_text();marker=' printf("FACE_ATTACHMENT_PASS'
extra=r"""
 // Existing view must not retain write permission after original Loader leaves visual route.
 modeValue=0;master=false;options=0;sceneRoot=make();auto*detached=qobject_cast<QQuickItem*>(sceneRoot->findChild<QObject*>("ControlScreen_popupLoader"));C(detached);detached->setParentItem(nullptr);before=writes;C(active->key(FacePopupKey::Previous));C(active->key(FacePopupKey::Accept));fprintf(stderr,"STALE_VISUAL_ROUTE writes_before=%u after=%u\n",before,writes);C(writes==before);active->close();delete active;active=nullptr;delete sceneRoot;
 modeValue=0;master=false;options=0;sceneRoot=make();auto*orphan=qobject_cast<QQuickItem*>(sceneRoot->findChild<QObject*>("PopoverFaceDetection_root"));C(orphan);orphan->setParentItem(nullptr);before=writes;C(active->key(FacePopupKey::Previous));C(active->key(FacePopupKey::Accept));fprintf(stderr,"STALE_ORIGINAL_ROUTE writes_before=%u after=%u\n",before,writes);C(writes==before);active->close();delete active;active=nullptr;delete sceneRoot;
"""
assert marker in t;t=t.replace(marker,extra+marker);p.write_text(t)
svg=b.OUT/'native-ui/screen-svg-deps/sdk/6.4.1/mingw_64';env=dict(os.environ,TEMP=str(out),TMP=str(out),PATH=str(svg/'bin')+os.pathsep+str(b.CXX.parent)+os.pathsep+str(b.SDK/'bin')+os.pathsep+os.environ['PATH'],QT_PLUGIN_PATH=str(svg/'plugins')+os.pathsep+str(b.SDK/'plugins'),QML2_IMPORT_PATH=str(out/'private-imports')+os.pathsep+str(b.SDK/'qml'),QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',QML_DISABLE_DISK_CACHE='1')
modules=['Test','Quick','Qml','QmlModels','Gui','Network','Core'];inc=['-I'+str(b.SDK/'include'),'-I'+str(src)]
for m in modules:inc+=['-I'+str(b.SDK/'include'/('Qt'+m)),'-I'+str(b.SDK/'include'/('Qt'+m)/'6.4.1'),'-I'+str(b.SDK/'include'/('Qt'+m)/'6.4.1'/('Qt'+m))]
cmd=[b.CXX,'-std=c++17','-O2','-fno-builtin','-DQT_GUI_LIB','-DQT_NO_KEYWORDS','-DQT_NO_DEBUG',*inc,*sorted(src.glob('*.cpp')),*[b.SDK/'lib'/('libQt6'+m+'.a') for m in modules],'-o',out/'test.exe'];r=subprocess.run(list(map(str,cmd)),env=env,capture_output=True,text=True,timeout=120);(out/'build.log').write_text(r.stdout+r.stderr);assert not r.returncode,r.stderr
ctypes.windll.kernel32.SetErrorMode(0x8003);r=subprocess.run([str(out/'test.exe'),str(out/'fixture.rcc'),str(out)],env=env,capture_output=True,text=True,timeout=90);(out/'run.log').write_text(r.stdout+r.stderr);(out/'report.json').write_text(json.dumps(dict(identity=ids,origin=str(base),exit=r.returncode,stdout=r.stdout,stderr=r.stderr,passed=not r.returncode,hardware_operations=0),indent=2));print(out);print(r.returncode,r.stdout,r.stderr)
