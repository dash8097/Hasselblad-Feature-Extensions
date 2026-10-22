"""Independent frozen actual-Qt face attachment checks; no camera or real FaceIO."""
from pathlib import Path
import sys,os,subprocess,hashlib,json,shutil,ctypes
sys.dont_write_bytecode=True
import build_processing_tests as b

def main():
 assert Path.cwd().resolve()==b.ROOT
 base=b.OUT/'native-ui/face-live-attachment'/sys.argv[1]
 ids={'checker':b.sha(Path(__file__)),'base':b.sha(base/'report.json')}
 out=b.OUT/'checker-ui/face-live-attachment'/hashlib.sha256(json.dumps(ids,sort_keys=True).encode()).hexdigest()[:20]
 src=out/'private-source';shutil.copytree(base/'private-source',src)
 for n in ('fixture.rcc','extra.rcc','private-font.ttc','scene.qml'):(out/n).write_bytes((base/n).read_bytes())
 shutil.copytree(base/'private-imports',out/'private-imports')
 p=src/'face_live_attachment_test.cpp';t=p.read_text();t=t.replace('C(argc==3);','C(argc==4);setbuf(stderr,nullptr);int scenario=atoi(argv[3]);')
 marker=' printf("FACE_LIVE_ATTACHMENT_PASS'
 extra=r'''
 // The same actual Qt scene and signal bridge, fresh attachment for each process.
 r=scene();FaceLiveAttachment independent(api,&abi,stock,menu,binding,sig,f,keys,map,reinterpret_cast<X2dFaceIo*>(&fakeIO),10);
 C(independent.tick(w.contentItem(),0)==0&&independent.hooks_ready());
 auto*originalAf=r->findChild<QQuickItem*>("AfIndicator_root");
 auto*originalFace=r->findChild<QQuickItem*>("FaceIndicator_root");
 auto*originalPad=r->findChild<QQuickItem*>("LiveViewTouchpad_root");
 auto*ownMouse=r->findChild<QQuickItem*>("X2dNativeFaceDrag");
 auto*ownLayer=r->findChild<QQuickItem*>("X2dNativeFaceLayer");C(originalAf&&originalFace&&originalPad&&ownMouse&&ownLayer);
 fprintf(stderr,"INDEPENDENT_LIVE_CASE %d BEGIN\n",scenario);
 if(scenario==0){auto*replacement=new QQuickItem;replacement->setParent(r);C(originalAf->setProperty("viewModel",QVariant::fromValue<QQuickItem*>(replacement)));}
 if(scenario==1)C(originalAf->setProperty("viewModel",QVariant::fromValue<QQuickItem*>(nullptr)));
 if(scenario==2)originalAf->setParentItem(nullptr);
 if(scenario==3)originalFace->setParentItem(nullptr);
 if(scenario==4)originalPad->setParentItem(nullptr);
 if(scenario==5)r->setParentItem(nullptr);
 if(scenario>=6&&scenario<=9){auto*duplicate=new QQuickItem(r);const char*names[]={"AfIndicator_root","FaceIndicator_root","LiveViewTouchpad_root","LiveViewOverlay_root"};duplicate->setObjectName(names[scenario-6]);}
 if(scenario==10)C(r->setProperty("inIdleState",false));
 if(scenario==11)C(r->setProperty("faceDetectionPaused",true));
 if(scenario==12)C(r->setProperty("screenBack",false));
 if(scenario==13)C(r->setProperty("canMoveFocusTp",false));
 if(scenario==14)C(r->setProperty("faceDetectionMulti",false));
 if(scenario<=14){unsigned before=writes;int oldUpdates=r->property("updates").toInt();auto oldPoint=r->property("setPoint");QQuickMouseEvent event;event.reset(500,300,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier,true);C(QMetaObject::invokeMethod(ownMouse,"clicked",Qt::DirectConnection,Q_ARG(QQuickMouseEvent*,&event)));fprintf(stderr,"LIVE_INPUT writes_before=%u after=%u updates_before=%d after=%d\n",before,writes,oldUpdates,r->property("updates").toInt());C(writes==before&&r->property("updates").toInt()==oldUpdates&&r->property("setPoint")==oldPoint);}
 if(scenario==15){C(originalAf->setProperty("viewModel",QVariant::fromValue<QQuickItem*>(nullptr)));int oldUpdates=r->property("updates").toInt();C(QMetaObject::invokeMethod(ownMouse,"canceled",Qt::DirectConnection));fprintf(stderr,"STALE_CANCEL updates_before=%d after=%d\n",oldUpdates,r->property("updates").toInt());C(r->property("updates").toInt()==oldUpdates);}
 if(scenario==16){QObject::connect(ownLayer,&QQuickItem::widthChanged,&w,[&](){independent.close();});originalAf->setWidth(1279);independent.tick(w.contentItem(),50);C(!independent.hooks_ready());}
 if(scenario==17){QObject::connect(ownLayer,&QQuickItem::widthChanged,&w,[ownLayer](){delete ownLayer;});originalAf->setWidth(1279);independent.tick(w.contentItem(),50);C(!independent.hooks_ready());}
 independent.close();C(independent.drain());delete r;C(w.contentItem()->childItems().isEmpty());fprintf(stderr,"INDEPENDENT_LIVE_CASE %d END\n",scenario);
'''
 assert marker in t;t=t.replace(marker,extra+marker);p.write_text(t,encoding='utf-8')
 svg=b.OUT/'native-ui/screen-svg-deps/sdk/6.4.1/mingw_64'
 env=dict(os.environ,TEMP=str(out),TMP=str(out),PATH=str(svg/'bin')+os.pathsep+str(b.CXX.parent)+os.pathsep+str(b.SDK/'bin')+os.pathsep+os.environ['PATH'],QT_PLUGIN_PATH=str(svg/'plugins')+os.pathsep+str(b.SDK/'plugins'),QML2_IMPORT_PATH=str(out/'private-imports')+os.pathsep+str(b.SDK/'qml'),QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',QML_DISABLE_DISK_CACHE='1')
 modules=['Test','Quick','Qml','QmlModels','Gui','Network','Core'];inc=['-I'+str(b.SDK/'include'),'-I'+str(src)]
 for m in modules:inc+=['-I'+str(b.SDK/'include'/('Qt'+m)),'-I'+str(b.SDK/'include'/('Qt'+m)/'6.4.1'),'-I'+str(b.SDK/'include'/('Qt'+m)/'6.4.1'/('Qt'+m))]
 sources=[p for p in sorted(src.glob('*.cpp')) if p.name!='face_live_profile_test.cpp']
 cmd=[b.CXX,'-std=c++17','-O2','-fno-builtin','-DQT_GUI_LIB','-DQT_NO_KEYWORDS','-DQT_NO_DEBUG',*inc,*sources,*[b.SDK/'lib'/('libQt6'+m+'.a') for m in modules],'-o',out/'test.exe']
 r=subprocess.run(list(map(str,cmd)),env=env,capture_output=True,text=True,timeout=120);(out/'build.log').write_text(r.stdout+r.stderr,encoding='utf-8');assert not r.returncode,r.stderr
 ctypes.windll.kernel32.SetErrorMode(0x8003);results=[]
 for case in range(18):
  r=subprocess.run([str(out/'test.exe'),str(out/'fixture.rcc'),str(out),str(case)],env=env,capture_output=True,text=True,timeout=45)
  (out/f'case-{case}.log').write_text(r.stdout+r.stderr,encoding='utf-8');results.append(dict(case=case,exit=r.returncode,passed=not r.returncode));print(results[-1],flush=True)
 report=dict(identity=ids,source_freeze=str(base),source_sha256={p.name:b.sha(p) for p in sources if '_test' not in p.name},results=results,actual_Qt=True,hardware_operations=0,FaceIO='explicit unavailable standin',original_ARM_GUI=False)
 (out/'report.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print(out)
if __name__=='__main__':main()
