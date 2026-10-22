from pathlib import Path
import ctypes,hashlib,json,os,subprocess,sys
sys.dont_write_bytecode=True
import build_processing_tests as b

def main():
 assert Path.cwd().resolve()==b.ROOT
 kind=sys.argv[1];freeze=sys.argv[2] if len(sys.argv)>2 else ('989dfe6509ae78de8769' if kind=='draw' else 'a60d352765f6d1bca3bb');group='face-draw-pool' if kind=='draw' else 'face-live-profile';base=b.OUT/'native-ui'/group/freeze
 ids={'checker':b.sha(Path(__file__)),'base':b.sha(base/'report.json'),'kind':kind};out=b.OUT/'checker-ui'/group/hashlib.sha256(json.dumps(ids,sort_keys=True).encode()).hexdigest()[:20];src=out/'private-source';src.mkdir(parents=True,exist_ok=False)
 for p in (base/'private-source').iterdir():(src/p.name).write_bytes(p.read_bytes())
 (out/'private-font.ttc').write_bytes((base/'private-font.ttc').read_bytes())
 if kind=='draw':
  p=src/'face_draw_pool_test.cpp';t=p.read_text();t=t.replace('C(argc==2);','C(argc==3);setbuf(stderr,nullptr);int scenario=atoi(argv[2]);')
  t=t.replace('static unsigned checks;', '''static void(*saved_name)(void*,void*,const char*);
static void destructive_name(void*c,void*o,const char*n){auto*q=static_cast<QObject*>(o);QObject::connect(q,&QObject::objectNameChanged,q,[q](){delete q;});saved_name(c,o,n);}
static unsigned checks;''')
  marker='printf("FACE_DRAW_POOL_PASS'
  extra=r"""
 if(scenario==6){auto destructive=api;saved_name=api.name;destructive.name=destructive_name;fprintf(stderr,"CREATE_NAME_DESTROY_BEFORE\n");p=FaceDrawPool::create(destructive,stock,colors,w.contentItem());C(!p||!p->item());if(p)p->close();C(w.contentItem()->childItems().empty());fprintf(stderr,"CREATE_NAME_DESTROY_AFTER\n");return 0;}
 f.count=1;f.width=320;f.height=240;f.commands[0]={FaceDrawKind::Rectangle,1,1,2,2,0,0,0xffffffff,0};p=create();C(p->update(f));r=static_cast<QQuickItem*>(p->item());fprintf(stderr,"INDEPENDENT_SCENARIO %d BEFORE\n",scenario);
 if(scenario==0){QObject::connect(r,&QQuickItem::widthChanged,&w,[&](){auto*old=p;p=nullptr;old->close();});f.width=321;bool ok=p->update(f);C(!ok&&!p);}
 if(scenario==1){QObject::connect(r,&QQuickItem::widthChanged,&w,[r](){delete r;});f.width=321;C(!p->update(f));p->close();p=nullptr;}
 if(scenario==2){auto*child=r->childItems()[0];QObject::connect(child,&QQuickItem::widthChanged,&w,[&](){auto*old=p;p=nullptr;old->close();});f.commands[0].width=3;bool ok=p->update(f);C(!ok&&!p);}
 if(scenario==3){auto*child=r->childItems()[0];QObject::connect(child,&QQuickItem::widthChanged,&w,[child](){delete child;});f.commands[0].width=3;C(!p->update(f));p->close();p=nullptr;}
 if(scenario==4){QObject::connect(r,&QQuickItem::visibleChanged,&w,[&](){auto*old=p;p=nullptr;old->close();});f.valid=false;C(!p->update(f)&&!p);}
 if(scenario==5){auto*child=r->childItems()[0];QObject::connect(child,&QObject::destroyed,&w,[&](){auto*old=p;p=nullptr;old->close();});f.commands[0]={FaceDrawKind::Text,1,1,20,20,0,0,0xffffffff,0,u"x",1,12};C(!p->update(f)&&!p);}
 C(w.contentItem()->childItems().empty());fprintf(stderr,"INDEPENDENT_SCENARIO %d AFTER\n",scenario);
"""
  assert marker in t;t=t.replace(marker,extra+marker);p.write_text(t)
  names=['face_draw_pool.cpp','face_draw_pool_test.cpp','abi_view_api.cpp','stock_factory.cpp','qt_stock_profile.cpp','qt_abi_profile.cpp'];wrap=['-Wl,--wrap=malloc','-Wl,--wrap=calloc'];runs=range(7)

 else:
  p=src/'face_live_profile_test.cpp';t=p.read_text();t=t.replace('#include <cstdlib>','#include <cstdlib>\n#include <limits>');t=t.replace('ListModel {Component','ListModel {dynamicRoles:true;Component');t=t.replace('function duplicate(){','function badRole(name,value){faceRegionList.setProperty(0,name,value)}\nfunction duplicate(){')
  marker='cam=nullptr;'
  extra=r"""
 {
 C(QMetaObject::invokeMethod(vm,"replaceFaces",Q_ARG(QVariant,2)));
 auto roleCase=[&](const char*name,const QVariant&value,bool expected){C(QMetaObject::invokeMethod(vm,"replaceFaces",Q_ARG(QVariant,2)));C(QMetaObject::invokeMethod(vm,"badRole",Q_ARG(QVariant,QString(name)),Q_ARG(QVariant,value)));bool ok=face_live_snapshot(p,s,m,vm,1280,960,snap,mode);C(ok==expected);if(!ok)C(!snap.present&&snap.count==0);};
 roleCase("id",65534,true);roleCase("id",-1,false);roleCase("id",65535,false);roleCase("id",.5,false);roleCase("id",QString("9"),false);roleCase("id",true,false);roleCase("id",QVariantMap{{"value",9}},false);
 for(auto name:{"id","x","y","width","height"}){roleCase(name,std::numeric_limits<double>::infinity(),false);roleCase(name,std::numeric_limits<double>::quiet_NaN(),false);roleCase(name,QString("1"),false);roleCase(name,-1,false);}
 roleCase("x",9995,false);roleCase("y",9995,false);roleCase("width",0,false);roleCase("height",0,false);roleCase("width",10000,false);roleCase("height",10000,false);roleCase("x",0,true);roleCase("y",0,true);roleCase("width",9900,true);roleCase("height",9900,true);
 C(QMetaObject::invokeMethod(vm,"replaceFaces",Q_ARG(QVariant,2)));auto oldRect=vm->property("focusRegion");vm->setProperty("focusRegion",QRectF(0,0,std::numeric_limits<double>::infinity(),1));C(!face_live_snapshot(p,s,m,vm,1280,960,snap,mode)&&!snap.present);vm->setProperty("focusRegion",oldRect);auto oldPoint=vm->property("focusRegionCentrePoint");for(auto point:{QPointF(-.1,.5),QPointF(1.1,.5),QPointF(.5,std::numeric_limits<double>::quiet_NaN())}){vm->setProperty("focusRegionCentrePoint",point);C(!face_live_snapshot(p,s,m,vm,1280,960,snap,mode));}vm->setProperty("focusRegionCentrePoint",oldPoint);
 auto oldList=vm->property("faceRegionList");QObject wrongList;C(vm->setProperty("faceRegionList",QVariant::fromValue<QObject*>(&wrongList)));C(!face_live_snapshot(p,s,m,vm,1280,960,snap,mode));C(vm->setProperty("faceRegionList",oldList));C(face_live_snapshot(p,s,m,vm,1280,960,snap,mode));
 auto oldWrites=writes;for(auto val:{-1,65535,0x40000000-1,0x4000ffff,0x40010001,0x40020006})C(!face_live_apply(p,s,vm,vm,{FaceActionKind::FaceTarget,val}));C(writes==oldWrites);
 for(auto val:{0,65534,0x40000000,0x4000fffe,0x40010000,0x40020000,0x40020005})C(face_live_apply(p,s,vm,vm,{FaceActionKind::FaceTarget,val}));
 C(!face_live_apply(p,s,vm,vm,{FaceActionKind::FocusPoint,0,std::numeric_limits<double>::infinity(),0}));C(!face_live_apply(p,s,vm,vm,{FaceActionKind::AfPosUpdated,0,16385,0}));
 QObject*detected=vm->property("detectedFace").value<QObject*>();C(detected);C(detected->setProperty("faceId",-3));C(!face_live_snapshot(p,s,m,vm,1280,960,snap,mode)&&!snap.present);C(detected->setProperty("faceId",-1));C(detected->setProperty("regionValid",false));C(face_live_snapshot(p,s,m,vm,1280,960,snap,mode)&&snap.detectedId==-1&&!snap.detectedValid);C(detected->setProperty("regionValid",true));C(detected->setProperty("faceId",-2));C(face_live_snapshot(p,s,m,vm,1280,960,snap,mode));C(snap.present&&snap.detectedValid&&snap.detectedId==-2);auto sentinelWrites=writes;C(!face_live_apply(p,s,vm,vm,{FaceActionKind::FaceTarget,-2}));C(!face_live_apply(p,s,vm,vm,{FaceActionKind::FaceTarget,-1}));C(writes==sentinelWrites);C(detected->setProperty("faceId",9));C(face_live_snapshot(p,s,m,vm,1280,960,snap,mode)&&snap.detectedId==9);fprintf(stderr,"FACTORY_NEGATIVE_TWO_SENTINEL_ACCEPTED_WITHOUT_NEGATIVE_SETTER\n");
 fprintf(stderr,"INDEPENDENT_PROFILE_BOUNDARIES_DONE\n");
 }
"""
  assert marker in t;t=t.replace(marker,extra+marker);p.write_text(t)
  names=['face_live_profile.cpp','face_live_profile_test.cpp','face_factory.cpp','qt_menu_profile.cpp','abi_view_api.cpp','stock_factory.cpp','qt_stock_profile.cpp','qt_abi_profile.cpp'];wrap=[];runs=range(1)

 env=dict(os.environ,TEMP=str(out),TMP=str(out),PATH=str(b.CXX.parent)+os.pathsep+str(b.SDK/'bin')+os.pathsep+os.environ['PATH'],QT_PLUGIN_PATH=str(b.SDK/'plugins'),QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',QML_DISABLE_DISK_CACHE='1')
 modules=['Quick','Qml','QmlModels','Gui','Network','Core'];inc=['-I'+str(b.SDK/'include'),'-I'+str(src)]
 for m in modules:inc+=['-I'+str(b.SDK/'include'/('Qt'+m)),'-I'+str(b.SDK/'include'/('Qt'+m)/'6.4.1'),'-I'+str(b.SDK/'include'/('Qt'+m)/'6.4.1'/('Qt'+m))]
 cmd=[b.CXX,'-std=c++17','-O2','-fno-builtin','-DQT_NO_KEYWORDS','-DQT_NO_DEBUG',*inc,*[src/n for n in names],*[b.SDK/'lib'/('libQt6'+m+'.a') for m in modules],*wrap,'-o',out/'test.exe'];r=subprocess.run(list(map(str,cmd)),env=env,capture_output=True,text=True,timeout=120);(out/'build.log').write_text(r.stdout+r.stderr);assert not r.returncode,r.stderr
 ctypes.windll.kernel32.SetErrorMode(0x8003);results=[]
 for case in runs:
  r=subprocess.run([str(out/'test.exe'),str(out),str(case)],env=env,capture_output=True,text=True,timeout=45);(out/f'case-{case}.log').write_text(r.stdout+r.stderr);results.append(dict(case=case,exit=r.returncode,passed=not r.returncode));print(results[-1])
 (out/'report.json').write_text(json.dumps(dict(identity=ids,frozen_origin=str(base),production_sources={n:b.sha(src/n) for n in names if 'test' not in n},results=results,actual_Qt=True,hardware_operations=0),indent=2));print(out)
if __name__=='__main__':main()
