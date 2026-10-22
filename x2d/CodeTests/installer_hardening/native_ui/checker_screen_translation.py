"""Small independent translation ownership and notification-close regression."""
from pathlib import Path
import os,sys,json,hashlib,shutil,subprocess,ctypes
sys.dont_write_bytecode=True
import build_processing_tests as b

def main():
 assert Path.cwd().resolve()==b.ROOT
 mode=sys.argv[1];base=b.OUT/'native-ui'/('translation' if mode=='translation' else 'screen-mode-attachment')/sys.argv[2]
 identity={'checker':b.sha(Path(__file__)),'base':b.sha(base/'report.json'),'mode':mode}
 out=b.OUT/'checker-ui/screen-translation'/hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()[:20]
 src=out/'private-source';shutil.copytree(base/'private-source',src)
 if mode=='translation':
  p=src/'factory_translation_test.cpp';t=p.read_text();marker='p.translate=nullptr;'
  extra=r'''{auto*temporary=new Translator;temporary->value=QString::fromUtf16(u"ファインダーのみ \U0001f50d");C(app.installTranslator(temporary));auto owned=factory_screen_notice(p);C(owned.valid()&&owned==u"ファインダーのみ \U0001f50d");delete temporary;C(owned==u"ファインダーのみ \U0001f50d");C(factory_screen_notice(p)==u"Live View in EVF Only");}'''
  assert marker in t;t=t.replace(marker,extra+marker);p.write_text(t,encoding='utf-8');modules=['Core'];files=['factory_translation.cpp','factory_translation_test.cpp'];argv=[]
 else:
  for n in ('fixture.rcc','extra.rcc'):(out/n).write_bytes((base/n).read_bytes())
  shutil.copytree(base/'private-imports',out/'private-imports')
  p=src/'screen_mode_attachment_test.cpp';t=p.read_text();marker=' delete state;printf('
  extra=r'''{
  state->setProperty("keyOverride",1);activeScreen=1;
  ScreenModeAttachment languageClose(api,qt_stock_profile(),qt_menu_profile(),qt_signal_profile(),p,kp,live,km,u"Notice");
  C(languageClose.tick(win.contentItem(),now+=50,800,600)==0);C(languageClose.tick(win.contentItem(),now+=50,800,600)==0);
  auto*noticeRoot=win.contentItem()->findChild<QQuickItem*>("X2dNativeScreenMode");C(noticeRoot&&noticeRoot->childItems().size()==1);auto*label=noticeRoot->childItems()[0];
  auto*closeWatch=signal_watch(qt_signal_profile(),label,label,"textChanged()",[](void*x){static_cast<ScreenModeAttachment*>(x)->close();},&languageClose,[](void*){});C(closeWatch);
  C(languageClose.set_notice(u"ファインダーのみ \U0001f50d"));languageClose.tick(win.contentItem(),now+=50,800,600);
  C(!languageClose.hooks_ready()&&!win.contentItem()->findChild<QQuickItem*>("X2dNativeScreenMode"));C(!languageClose.set_notice(u"No reopen"));signal_unwatch(closeWatch);
 }
'''
  assert marker in t;t=t.replace(marker,extra+marker);p.write_text(t,encoding='utf-8')
  modules=['Quick','Qml','QmlModels','Gui','Network','Core'];files=[p.name for p in src.glob('*.cpp') if p.name!='screen_factory_test.cpp'];argv=[str(out/'fixture.rcc'),str(out)]
 svg=b.OUT/'native-ui/screen-svg-deps/sdk/6.4.1/mingw_64'
 env=dict(os.environ,TEMP=str(out),TMP=str(out),PATH=str(svg/'bin')+os.pathsep+str(b.CXX.parent)+os.pathsep+str(b.SDK/'bin')+os.pathsep+os.environ['PATH'],QT_PLUGIN_PATH=str(svg/'plugins')+os.pathsep+str(b.SDK/'plugins'),QML2_IMPORT_PATH=str(out/'private-imports')+os.pathsep+str(b.SDK/'qml'),QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',QML_DISABLE_DISK_CACHE='1')
 inc=['-I'+str(b.SDK/'include'),'-I'+str(src)]
 for m in modules:inc+=['-I'+str(b.SDK/'include'/('Qt'+m)),'-I'+str(b.SDK/'include'/('Qt'+m)/'6.4.1'),'-I'+str(b.SDK/'include'/('Qt'+m)/'6.4.1'/('Qt'+m))]
 cmd=[b.CXX,'-std=c++17','-O2','-fno-builtin','-DQT_NO_KEYWORDS','-DQT_NO_DEBUG',*inc,*[src/n for n in files],*[b.SDK/'lib'/('libQt6'+m+'.a') for m in modules],'-o',out/'test.exe']
 r=subprocess.run(list(map(str,cmd)),env=env,capture_output=True,text=True,timeout=120);(out/'build.log').write_text(r.stdout+r.stderr,encoding='utf-8');assert not r.returncode,r.stderr
 ctypes.windll.kernel32.SetErrorMode(0x8003);r=subprocess.run([str(out/'test.exe'),*argv],env=env,capture_output=True,text=True,timeout=45)
 (out/'run.log').write_text(r.stdout+r.stderr,encoding='utf-8');(out/'report.json').write_text(json.dumps(dict(identity=identity,source_freeze=str(base),exit=r.returncode,passed=not r.returncode,stdout=r.stdout,stderr=r.stderr,actual_desktop_Qt=True,camera_operations=0),ensure_ascii=False,indent=2),encoding='utf-8');print(out);print(r.returncode,r.stdout,r.stderr)
if __name__=='__main__':main()
