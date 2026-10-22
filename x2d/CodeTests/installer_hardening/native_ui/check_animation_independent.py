"""Independent replay of frozen animation view and extra attachment scenarios. No device."""
from pathlib import Path
import sys,hashlib,json,os,subprocess,ctypes
sys.dont_write_bytecode=True
import build_processing_tests as b
BASE=b.OUT/'native-ui'
ADDED=r"""
 root=make(engine,window);drawer=root->findChild<QQuickItem*>("ControlDrawer_root");effect.ready=0;effect.mode=2;
 AnimationAttachment independent(api,stock,menu,signal,imageProfile(),&effect,{nullptr,[](void*){return timeNow;}});
 auto itick=[&](uint64_t t){timeNow=t;return independent.tick(root,t,800,600);};
 before=plays;check(itick(2000),"independent late preference source bound");timeNow=2001;drawer->setProperty("mainState","exposing");check(!independent.active()&&plays==before,"unready rising edge never plays");effect.ready=1;check(itick(2010)&&!independent.active()&&plays==before,"late preference readiness does not manufacture edge");
 timeNow=2020;drawer->setProperty("mainState","liveview");timeNow=2021;drawer->setProperty("mainState","exposing");check(independent.active()&&plays==before+1,"next genuine signal edge plays once");check(itick(2030),"independent visible frame");settle(80);auto*iv=root->findChild<QQuickItem*>("X2dNativeShutterAnimation");check(iv&&iv->isVisible(),"independent real view visible");
 check(itick(2005)&&!independent.active()&&plays==before+1,"backward clock stops without replay");settle(80);check(iv->opacity()<=.001,"backward clock retains only original faint initialization frame");timeNow=2100;drawer->setProperty("mainState","liveview");timeNow=2101;drawer->setProperty("mainState","exposing");check(independent.active()&&plays==before+2,"clock recovery requires new edge");check(itick(4101)&&!independent.active()&&plays==before+2,"exact 2000ms watchdog hides without replay");check(itick(4200)&&plays==before+2,"long same exposure remains stopped");
 effect.mode=1;timeNow=4300;drawer->setProperty("mainState","liveview");check(itick(4301),"silent mode poll");timeNow=4302;drawer->setProperty("mainState","exposing");check(independent.active()&&plays==before+2,"silent new edge");effect.mode=2;check(itick(4310)&&plays==before+2,"switch to audio during exposure never replays");
 auto*old=root;root=make(engine,window);drawer=root->findChild<QQuickItem*>("ControlDrawer_root");check(itick(4400)&&independent.has_source()&&!independent.active(),"new root discovers clean source");check(!old->findChild<QObject*>("X2dNativeShutterAnimation"),"root switch removes old overlay");delete old;timeNow=4401;drawer->setProperty("mainState","exposing");check(independent.active()&&plays==before+3,"new root edge runs once");independent.close();check(!root->findChild<QObject*>("X2dNativeShutterAnimation")&&!effect.enabled,"explicit close tears down source and preferences");delete root;root=nullptr;
"""
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run_one(kind,rev,extra):
 frozen=BASE/kind/rev;report=json.loads((frozen/'report.json').read_text(encoding='utf8'));out=b.OUT/'checker-routes'/('animation-independent-v2-'+rev[:8]);src=out/'private-source';src.mkdir(parents=True,exist_ok=False)
 for p in (frozen/'private-source').iterdir():
  if p.is_file():
   if p.name in report['identity']:assert sha(p)==report['identity'][p.name],p.name
   (src/p.name).write_bytes(p.read_bytes())
 if extra:
  q=src/'test_animation_attachment.cpp';s=q.read_text(encoding='utf8');mark=' printf("NATIVE_ANIMATION_ATTACHMENT_PASS';assert mark in s;s=s.replace(mark,ADDED+'\n'+mark);q.write_text(s,encoding='utf8')
 mods=['Quick','Qml','QmlModels','Gui','Network','Core'];inc=['-I'+str(b.SDK/'include')]
 for m in mods:inc+=['-I'+str(b.SDK/'include'/('Qt'+m)),'-I'+str(b.SDK/'include'/('Qt'+m)/'6.4.1'),'-I'+str(b.SDK/'include'/('Qt'+m)/'6.4.1'/('Qt'+m))]
 svg=BASE/'screen-svg-deps/sdk/6.4.1/mingw_64';env=dict(os.environ,TEMP=str(out),TMP=str(out),PATH=str(svg/'bin')+os.pathsep+str(b.CXX.parent)+os.pathsep+str(b.SDK/'bin')+os.pathsep+os.environ['PATH'],QT_PLUGIN_PATH=str(svg/'plugins')+os.pathsep+str(b.SDK/'plugins'),QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',QSG_RENDER_LOOP='basic',QML_DISABLE_DISK_CACHE='1',QML2_IMPORT_PATH=str(b.SDK/'qml'))
 cmd=[b.CXX,'-std=c++17','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-DQT_NO_DEBUG','-DQT_NO_KEYWORDS',*inc,*sorted(src.glob('*.cpp')),*[b.SDK/'lib'/('libQt6'+m+'.a') for m in mods],'-o',out/'test.exe']
 r=subprocess.run(list(map(str,cmd)),env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=120);(out/'build.log').write_text(r.stdout+r.stderr,encoding='utf8');assert not r.returncode,r.stderr
 r=subprocess.run([str(out/'test.exe'),report['oracle'],str(out)],env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=180);(out/'run.log').write_text(r.stdout+r.stderr,encoding='utf8')
 evidence=dict(frozen=str(frozen.relative_to(b.ROOT)),frozen_report_sha256=sha(frozen/'report.json'),source_sha256={p.name:sha(p) for p in src.iterdir() if p.is_file()},binary_sha256=sha(out/'test.exe'),exit=r.returncode,stdout=r.stdout,stderr=r.stderr,independent_extra_scenarios=5 if extra else 0,original_gui_executed=False,hardware_operations=0,limits='Real desktop Qt; original drawer and service state are explicit fixtures; original GUI and camera hardware not executed.')
 (out/'report.json').write_text(json.dumps(evidence,ensure_ascii=False,indent=2),encoding='utf8');print(out);print(r.stdout,r.stderr);assert not r.returncode
if __name__=='__main__':
 ctypes.windll.kernel32.SetErrorMode(0x8003)
 run_one('animation-attachment','d40f8426d5400cf39aec',True)
 run_one('native-animation-view','3679719f0f5bd75af257',False)
