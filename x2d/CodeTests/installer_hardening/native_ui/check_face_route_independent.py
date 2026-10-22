from pathlib import Path
import sys,json,os,subprocess,shutil,ctypes
sys.dont_write_bytecode=True
import build_processing_tests as b
revision=sys.argv[1] if len(sys.argv)>1 else '88822c1b3ae1b845ef82'
frozen=b.OUT/'native-ui/face-popup-attachment'/revision
out=b.OUT/'checker-routes'/('face-stale-route-v2-'+revision[:8]);src=out/'private-source';src.mkdir(parents=True,exist_ok=False)
r=json.loads((frozen/'report.json').read_text(encoding='utf8'))
for p in (frozen/'private-source').iterdir():
 if p.is_file():
  assert b.sha(p)==r['identity'][p.name]
  (src/p.name).write_bytes(p.read_bytes())
for n in ['fixture.rcc','extra.rcc']:(out/n).write_bytes((frozen/n).read_bytes())
shutil.copytree(frozen/'private-imports',out/'private-imports')
p=src/'face_popup_attachment_test.cpp';s=p.read_text(encoding='utf8');mark=' printf("FACE_ATTACHMENT_PASS';assert mark in s
s=s.replace(mark,r'''
 modeValue=0;master=false;options=0;sceneRoot=make();loader=sceneRoot->findChild<QObject*>("ControlScreen_popupLoader");orig=sceneRoot->findChild<QQuickItem*>("PopoverFaceDetection_root");C(active->key(FacePopupKey::Previous));before=writes;C(loader->setProperty("item",QVariant::fromValue<QQuickItem*>(nullptr)));C(!stock_get_object(stock,loader,"item"));C(orig&&active->attached());active->key(FacePopupKey::Accept);C(writes==before);active->close();delete active;active=nullptr;delete sceneRoot;
'''+mark);p.write_text(s,encoding='utf8')
svg=b.OUT/'native-ui/screen-svg-deps/sdk/6.4.1/mingw_64';env=dict(os.environ,TEMP=str(out),TMP=str(out),PATH=str(svg/'bin')+os.pathsep+str(b.CXX.parent)+os.pathsep+str(b.SDK/'bin')+os.pathsep+os.environ['PATH'],QT_PLUGIN_PATH=str(svg/'plugins')+os.pathsep+str(b.SDK/'plugins'),QML2_IMPORT_PATH=str(out/'private-imports')+os.pathsep+str(b.SDK/'qml'),QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',QML_DISABLE_DISK_CACHE='1')
modules=['Test','Quick','Qml','QmlModels','Gui','Network','Core'];inc=['-I'+str(b.SDK/'include'),'-I'+str(src)]
for m in modules:inc+=['-I'+str(b.SDK/'include'/('Qt'+m)),'-I'+str(b.SDK/'include'/('Qt'+m)/'6.4.1'),'-I'+str(b.SDK/'include'/('Qt'+m)/'6.4.1'/('Qt'+m))]
cmd=[b.CXX,'-std=c++17','-O2','-fno-builtin','-DQT_GUI_LIB','-DQT_NO_KEYWORDS','-DQT_NO_DEBUG',*inc,*sorted(src.glob('*.cpp')),*[b.SDK/'lib'/('libQt6'+m+'.a') for m in modules],'-o',out/'test.exe']
ctypes.windll.kernel32.SetErrorMode(0x8003);p=subprocess.run(list(map(str,cmd)),env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=120);(out/'build.log').write_text(p.stdout+p.stderr,encoding='utf8');assert not p.returncode,p.stderr
p=subprocess.run([str(out/'test.exe'),str(out/'fixture.rcc'),str(out)],env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=90);(out/'run.log').write_text(p.stdout+p.stderr,encoding='utf8');(out/'report.json').write_text(json.dumps(dict(frozen=str(frozen.relative_to(b.ROOT)),exit=p.returncode,stdout=p.stdout,stderr=p.stderr,independent_extra_cases=1,source_sha256={p.name:b.sha(p) for p in src.iterdir() if p.is_file()},hardware_operations=0),ensure_ascii=False,indent=2),encoding='utf8');print(out);print(p.stdout,p.stderr)
