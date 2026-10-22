from pathlib import Path
import os,sys,hashlib,json,subprocess,shutil
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[3];R=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7';Q=R/'native-ui/qt-sdk/6.4.1/mingw_64';C=R/'native-ui/host-mingw/sdk/Tools/mingw1120_64/bin/g++.exe';F=R/'checker-ui/settings-drive-actual-metrics-fixed'
cpp=['drive_attachment','drive_settings_session','drive_popup','drive_popup_view','multishot_settings','multishot_settings_view','pixel_controller','pixel_host_state','protocol_status','processing_presentation','processing_host','exposure_model_provider','processing_stock_view','key_filter','tree_observer','signal_bridge','stock_factory','menu_discovery','binding_lease','abi_view_api','qt_stock_profile','qt_menu_profile','qt_abi_profile','qt_signal_profile','qt_key_profile','qt_binding_profile']
paths=[HERE/(n+'.cpp') for n in cpp]+[HERE/'test_drive_attachment.cpp',HERE/'generate_drive_attachment_profile.py',Path(__file__).resolve()];headers=set()
def include(p):
 import re
 for n in re.findall(r'^#include "([^"]+)"',p.read_text(encoding='utf8'),re.M):
  q=HERE/n
  if q.exists() and q not in headers:headers.add(q);include(q)
for p in paths:include(p)
paths+=sorted(headers);sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();identity={p.name:sha(p) for p in paths};identity['fixture']=sha(F/'fixture.rcc');font=Path('C:/Windows/Fonts/msyh.ttc');identity['private_test_font']=sha(font);revision=hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()[:20];O=R/'native-ui/drive-attachment'/revision;S=O/'private-source';S.mkdir(parents=True,exist_ok=False)
for p in paths:shutil.copyfile(p,S/p.name)
assert all(sha(S/p.name)==identity[p.name] for p in paths)
sys.path.insert(0,str(HERE));sys.path.insert(0,str(ROOT/'x2d/CodeTests/temporary_af_speed_probe'))
from generate_drive_attachment_profile import generate
from inspect_menu_resources import load_gui
header,metadata=generate(load_gui());(O/'drive_profile.hpp').write_bytes(header);(O/'original-metadata.json').write_text(json.dumps(metadata,indent=2),encoding='utf8')
shutil.copytree(F/'private-fixture/imports',O/'private-imports');shutil.copyfile(F/'fixture.rcc',O/'fixture.rcc')
shutil.copyfile(font,O/'private-font.ttc')
env=dict(os.environ,TEMP=str(O),TMP=str(O),PATH=str(C.parent)+os.pathsep+str(Q/'bin')+os.pathsep+os.environ['PATH'],QML2_IMPORT_PATH=str(O/'private-imports')+os.pathsep+str(Q/'qml'),QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',QML_DISABLE_DISK_CACHE='1')
modules=['Quick','Qml','QmlModels','Gui','Network','Core','Test'];inc=['-I'+str(Q/'include'),'-I'+str(S)]
for m in modules:inc+=['-I'+str(Q/'include'/('Qt'+m)),'-I'+str(Q/'include'/('Qt'+m)/'6.4.1'),'-I'+str(Q/'include'/('Qt'+m)/'6.4.1'/('Qt'+m))]
cmd=[C,'-std=c++17','-O2','-fno-builtin','-DQT_GUI_LIB','-DQT_NO_KEYWORDS','-DQT_NO_DEBUG',*inc,*[S/(n+'.cpp') for n in cpp],S/'test_drive_attachment.cpp',*[Q/'lib'/('libQt6'+m+'.a') for m in modules],'-Wl,--wrap=malloc','-o',O/'test.exe'];p=subprocess.run(list(map(str,cmd)),env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=120);(O/'build.log').write_text(p.stdout+p.stderr,encoding='utf8');print(O);assert not p.returncode,p.stderr
p=subprocess.run([str(O/'test.exe'),str(O/'fixture.rcc'),str(O)],env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=120);(O/'run.log').write_text(p.stdout+p.stderr,encoding='utf8');(O/'report.json').write_text(json.dumps(dict(identity=identity,binary=sha(O/'test.exe'),stdout=p.stdout,stderr=p.stderr,exit=p.returncode,author_tests=True,independent_approval=False,scope='actual Qt6.4.1 stock TextValueRow/FramedImage/MenuHeader fixture; original control tree/proxy explicitly synthetic; no original GUI/USB/camera'),indent=2),encoding='utf8');print(p.stdout);assert not p.returncode,p.stderr
