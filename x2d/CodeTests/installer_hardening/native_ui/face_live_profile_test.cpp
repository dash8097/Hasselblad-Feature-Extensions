#include "face_live_profile.hpp"
#include "qt_stock_profile.hpp"
#include "qt_menu_profile.hpp"
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QtQmlModels/private/qqmllistmodel_p.h>
#include <QLibrary>
#include <QJSValue>
#include <QColor>
#include <cstdio>
#include <cstdlib>
using namespace x2dui;
static unsigned checks,writes;static int lastTarget=-1;static QObject camera,systemObject;
#define C(x) do{++checks;if(!(x)){fprintf(stderr,"FACE_LIVE_FAIL %d %s\n",__LINE__,#x);exit(1);}}while(0)
static void field(void*o,int op,int index,void**a){C(op==1);if(o==&camera){C(index==68||index==132);*static_cast<uint8_t*>(a[0])=index==68?2:1;}else{C(o==&systemObject);if(index==12)*static_cast<bool*>(a[0])=true;else if(index==13||index==32)*static_cast<uint32_t*>(a[0])=1;else{C(index==31);*static_cast<uint8_t*>(a[0])=0;}}}
template<class F>F fn(const char*n){auto r=QLibrary::resolve("Qt6Qml",n);if(!r)r=QLibrary::resolve("Qt6QmlModels",n);C(r);return reinterpret_cast<F>(r);}
static FaceLiveProfile profile(void**cam,void**sys){FaceLiveProfile p;auto&f=p.factory;f.proxy.slots[0]=cam;f.proxy.slots[1]=sys;f.proxy.meta[0]=f.proxy.meta[1]=&QObject::staticMetaObject;f.proxy.cast=[](const void*m,const void*o)->const void*{return static_cast<const QMetaObject*>(m)->cast(static_cast<const QObject*>(o));};f.proxy.call[0]=f.proxy.call[1]=field;f.face={0,68,1,false,false,true};f.master={1,12,1,true,false,true};f.options={1,13,4,false,false,true};f.modes={0,2,1};f.method_index=[](const void*m,const char*n){return static_cast<const QMetaObject*>(m)->indexOfMethod(n);};f.metacall=[](void*o,int c,int i,void**a){return QMetaObject::metacall(static_cast<QObject*>(o),QMetaObject::Call(c),i,a);};
 p.liveState={0,132,1,false,false,false};p.screenStatus={1,31,1,false,false,false};p.screensActive={1,32,4,false,false,false};p.running=2;p.runningCont=3;p.roiDefault=0;p.roiFace=1;p.roiLeft=2;p.roiRight=3;p.rectType=QMetaType::fromType<QRectF>().iface();p.pointType=QMetaType::fromType<QPointF>().iface();p.colorType=QMetaType::fromType<QColor>().iface();p.listMeta=&QQmlListModel::staticMetaObject;p.cameraDbusMeta=&QObject::staticMetaObject;p.face_target=[](void*o,int n){C(o==&camera);++writes;lastTarget=n;};
 // Explicit desktop model stand-in; the generated ARM profile binds VariantMapModel.
 p.list_get=[](const void*o,int i)->AbiVariant{auto row=static_cast<const QQmlListModel*>(o)->get(i);QVariantMap values;for(const char*n:{"id","x","y","width","height"})values.insert(QString::fromLatin1(n),row.property(QString::fromLatin1(n)).toVariant());AbiVariant out;new(&out)QVariant(values);return out;};
 p.mapType=QMetaType::fromType<QVariantMap>().iface();p.map_value=[](const void*o,const AbiString&key,const AbiVariant&fallback)->AbiVariant{AbiVariant out;auto k=QString::fromUtf16(key.data,key.size);new(&out)QVariant(static_cast<const QVariantMap*>(o)->value(k,*reinterpret_cast<const QVariant*>(&fallback)));return out;};return p;}
int main(int argc,char**argv){QGuiApplication app(argc,argv);QQmlEngine engine;QQmlComponent c(&engine);c.setData(R"(import QtQuick
Item { property bool faceDetectionSingle:false;property bool faceDetectionMulti:true;property bool faceDetectionPaused:false;property bool inIdleState:true;property bool inSession:false;property bool screenBack:true;property bool screenEVF:false;property bool isZoomed:false;property bool canMoveFocusTp:true;property bool evfTpMoveFocus:false;property bool touchpadPressed:false;property bool canShowAF:true
property int afStatus:0;property int afRoiType:1;property int afIndicatorStatus:3;property point focusRegionCentrePoint:Qt.point(.5,.5);property rect focusRegion:Qt.rect(.4,.4,.2,.2)
property QtObject detectedFace:QtObject {property bool regionValid:true;property int faceId:9;property rect region:Qt.rect(.3,.2,.2,.3);property rect leftEye:Qt.rect(.35,.25,.02,.02);property rect rightEye:Qt.rect(.42,.25,.02,.02)}
readonly property QtObject faceRegions:faceRegionList
property var faceRegionList:ListModel {Component.onCompleted:{append({id:9,x:3000,y:2000,width:2000,height:3000});append({id:11,x:6000,y:1000,width:1000,height:2000})}}
property point setPoint:Qt.point(-1,-1);property point shownPoint:Qt.point(-1,-1);property int updates:0;property bool shownActive:false
function setFocusPoint(pt){setPoint=pt}
function afPosUpdated(active,pos){shownActive=active;shownPoint=pos}
function updateAfPos(force){updates++}
function replaceFaces(n){faceRegionList.clear();for(var i=0;i<n;i++)faceRegionList.append({id:i,x:100,y:100,width:10,height:10})}
function duplicate(){faceRegionList.setProperty(1,"id",0)}
})",QUrl());if(c.isError())fprintf(stderr,"%s\n",qPrintable(c.errorString()));QObject*vm=c.create(engine.rootContext());C(vm);void*cam=&camera,*sys=&systemObject;auto p=profile(&cam,&sys);auto s=qt_stock_profile();auto m=qt_menu_profile();FaceSnapshot snap;FaceModeSnapshot mode;C(face_live_snapshot(p,s,m,vm,1280,960,snap,mode));C(snap.present&&snap.faceMode&&snap.eyeMode&&snap.selectionActive&&snap.foundFocus&&snap.count==2&&snap.candidates[1].id==11&&snap.detectedId==9&&snap.leftEye.x==.35&&snap.focusX==.5);
 auto*detected=vm->property("detectedFace").value<QObject*>();C(detected);C(detected->setProperty("faceId",-2));C(face_live_snapshot(p,s,m,vm,1280,960,snap,mode)&&snap.detectedId==-2);C(!face_live_apply(p,s,vm,vm,{FaceActionKind::FaceTarget,-2}));C(detected->setProperty("faceId",9));C(face_live_snapshot(p,s,m,vm,1280,960,snap,mode)&&snap.detectedId==9);
 auto goodGet=p.list_get;p.list_get=[](const void*,int)->AbiVariant{AbiVariant v;new(&v)QVariant(QVariantMap{{"id",1},{"x",1},{"y",1},{"width",1}});return v;};C(!face_live_snapshot(p,s,m,vm,1280,960,snap,mode));p.list_get=[](const void*,int)->AbiVariant{AbiVariant v;new(&v)QVariant(123);return v;};C(!face_live_snapshot(p,s,m,vm,1280,960,snap,mode));p.list_get=goodGet;
 C(face_live_apply(p,s,vm,vm,{FaceActionKind::FocusPoint,0,.25,.75}));C(vm->property("setPoint").toPointF()==QPointF(.25,.75));C(face_live_apply(p,s,vm,vm,{FaceActionKind::AfPosUpdated,1,200,300}));C(vm->property("shownPoint").toPointF()==QPointF(200,300)&&vm->property("shownActive").toBool());C(face_live_apply(p,s,vm,vm,{FaceActionKind::UpdateAfPos}));C(vm->property("updates")==1);C(face_live_apply(p,s,vm,vm,{FaceActionKind::FaceTarget,0x40020005}));C(writes==1&&lastTarget==0x40020005);C(!face_live_apply(p,s,vm,vm,{FaceActionKind::FaceTarget,0x40010001}));C(!face_live_apply(p,s,vm,vm,{FaceActionKind::FocusPoint,0,2,.5}));C(!face_live_apply(p,s,vm,vm,{FaceActionKind::ClosePopup}));
 for(int n=0;n<=65;n++){C(QMetaObject::invokeMethod(vm,"replaceFaces",Q_ARG(QVariant,n)));bool ok=face_live_snapshot(p,s,m,vm,1280,960,snap,mode);C(ok==(n<=64));if(ok)C(snap.count==size_t(n));}C(QMetaObject::invokeMethod(vm,"replaceFaces",Q_ARG(QVariant,2)));C(QMetaObject::invokeMethod(vm,"duplicate"));C(!face_live_snapshot(p,s,m,vm,1280,960,snap,mode));cam=nullptr;C(!face_live_snapshot(p,s,m,vm,1280,960,snap,mode));C(!face_live_apply(p,s,vm,vm,{FaceActionKind::FaceTarget,9}));delete vm;printf("FACE_LIVE_PROFILE_PASS assertions=%u\n",checks);return 0;
}
