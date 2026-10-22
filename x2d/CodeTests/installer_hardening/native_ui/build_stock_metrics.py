from pathlib import Path
import hashlib,json,os,subprocess,shutil
HERE=Path(__file__).resolve().parent; ROOT=HERE.parents[3]; BASE=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7'; SDK=BASE/'native-ui/qt-sdk/6.4.1/mingw_64'; CXX=BASE/'native-ui/host-mingw/sdk/Tools/mingw1120_64/bin/g++.exe'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
names='stock_metrics.cpp stock_metrics.hpp stock_factory.cpp stock_factory.hpp qt_stock_profile.cpp qt_stock_profile.hpp effect_view_core.cpp effect_view_core.hpp abi_view_api.cpp abi_view_api.hpp qt_abi_profile.cpp qt_abi_profile.hpp local_heap.hpp menu_discovery.hpp qt_menu_profile.cpp qt_menu_profile.hpp multishot_settings_view.hpp multishot_settings.hpp drive_popup_view.hpp drive_popup.hpp signal_bridge.hpp protocol_text.hpp pixel_controller.hpp pixel_host_state.hpp protocol_status.hpp processing_presentation.hpp test_stock_metrics.cpp build_stock_metrics.py'.split()
fixture=BASE/'checker-ui/settings-drive-actual-metrics-fixed/private-fixture'
identity={'sources':{n:sha(HERE/n) for n in names},'fixture':{p.relative_to(fixture).as_posix():sha(p) for p in fixture.rglob('*') if p.is_file()},'compiler':sha(CXX)}
rev=hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()[:20];O=BASE/'native-ui/stock-metrics'/rev;O.mkdir(parents=True,exist_ok=True);S=O/'private-source';S.mkdir(exist_ok=True)
for n in names:(S/n).write_bytes((HERE/n).read_bytes())
F=O/'private-fixture';shutil.copytree(fixture,F,dirs_exist_ok=True)
p=F/'imports/com/hasselblad/constants/Constants.qml';t=p.read_text();t=t.replace('property real scaleFactor:1;','property real scaleFactor:1;property real scaleFactorY:1;').replace('menuBackArrowLeftMargin:30','menuBackArrowLeftMargin:27.2*scaleFactorX').replace('externalMenuBackArrowLeftMargin:30','externalMenuBackArrowLeftMargin:27.2*scaleFactorX').replace('settingsMenuSubSectionHeaderTopMargin:115','settingsMenuSubSectionHeaderTopMargin:115*scaleFactorY').replace('settingsMenuExternalHeaderTopMargin:115','settingsMenuExternalHeaderTopMargin:89.6*scaleFactorY');p.write_text(t)
env=dict(os.environ,TEMP=str(O),TMP=str(O),PATH=str(CXX.parent)+os.pathsep+str(SDK/'bin')+os.pathsep+os.environ['PATH'],QML2_IMPORT_PATH=str(F/'imports')+os.pathsep+str(SDK/'qml'),QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',QML_DISABLE_DISK_CACHE='1')
entries=['<file alias="'+p.relative_to(F/'factory').as_posix()+'">'+p.as_posix()+'</file>' for p in (F/'factory').rglob('*') if p.is_file()];(O/'fixture.qrc').write_text('<RCC><qresource prefix="/app/qml">'+''.join(entries)+'</qresource></RCC>')
def run(name,cmd):
 p=subprocess.run(list(map(str,cmd)),env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=120);(O/(name+'.log')).write_text(p.stdout+p.stderr,encoding='utf8');assert p.returncode==0,p.stdout+p.stderr;return p
run('resource',[SDK/'bin/rcc.exe','--binary',O/'fixture.qrc','-o',O/'fixture.rcc'])
mods=['Quick','Qml','QmlModels','Gui','Network','Core'];inc=['-I'+str(SDK/'include'),'-I'+str(S)]
for m in mods:inc+=['-I'+str(SDK/'include'/('Qt'+m)),'-I'+str(SDK/'include'/('Qt'+m)/'6.4.1'),'-I'+str(SDK/'include'/('Qt'+m)/'6.4.1'/('Qt'+m))]
run('build',[CXX,'-std=c++17','-O2','-DQT_NO_DEBUG',*inc,*[S/n for n in names if n.endswith('.cpp')],*[SDK/'lib'/('libQt6'+m+'.a') for m in mods],'-o',O/'metrics.exe'])
p=run('run',[O/'metrics.exe',O/'fixture.rcc']);assert identity['sources']=={n:sha(HERE/n) for n in names};(O/'report.json').write_text(json.dumps({'identity':identity,'binary':sha(O/'metrics.exe'),'stdout':p.stdout,'camera_requests':0,'actual_camera_Qt':False,'Constants':'explicit dynamic fixture, formulas bound to original 4.2'},indent=2));print(O);print(p.stdout)
