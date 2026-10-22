#include "screen_factory_profile.hpp"
#include "qt_stock_profile.hpp"
#include <QCoreApplication>
#include <QObject>
#include <QStringList>
#include <QVariant>
#include <cstdio>
#include <cstdlib>
using namespace x2dui;
static bool chinese=false;
static QString localized(const QString&s){if(!chinese)return s;return s=="Auto"?QString::fromUtf16(u"自动"):s=="LCD"?QString::fromUtf16(u"仅显示屏"):s=="EVF"?QString::fromUtf16(u"仅取景器"):s=="off"?QString::fromUtf16(u"关"):s=="30 sec"?QString::fromUtf16(u"30秒"):s=="5 min"?QString::fromUtf16(u"5分钟"):s;}
static unsigned checks,writes;static bool evf,fpsValue;static int screens=1,timeout=300;
static QStringList display{QStringLiteral("Auto"),QStringLiteral("LCD"),QStringLiteral("EVF")},timing{QStringLiteral("off"),QStringLiteral("30 sec"),QStringLiteral("5 min")};
static QString fieldWritten,labelWritten;static unsigned proxyWritten;static QObject camera,systemObject,formatterObject;
#define C(v) do{++checks;if(!(v)){fprintf(stderr,"SCREEN_FACTORY_FAIL line=%d\n",__LINE__);exit(1);}}while(0)
static void proxyCall(void*o,int mode,int index,void**a){C(mode==1||mode==2);if(mode==2)++writes;
 if(o==&camera){bool*p=index==11?&fpsValue:index==38?&evf:nullptr;C(p);if(mode==1)*static_cast<bool*>(a[0])=*p;else *p=*static_cast<bool*>(a[0]);}
 else {C(o==&systemObject);int*p=index==33?&screens:index==30?&timeout:nullptr;C(p);if(mode==1)*static_cast<int*>(a[0])=*p;else *p=*static_cast<int*>(a[0]);}}
static void formatterCall(void*o,int mode,int index,void**a){C(o==&formatterObject&&mode==0);auto&name=*static_cast<QString*>(a[index==5?2:1]);C(name=="screens_allowed"||name=="screen_idle_timeout");
 if(index==4)*static_cast<QStringList*>(a[0])=name=="screens_allowed"?display:timing;
 else if(index==9)*static_cast<QString*>(a[0])=localized(*static_cast<QString*>(a[2]));
 else if(index==6){int value=static_cast<QVariant*>(a[2])->toInt();*static_cast<QString*>(a[0])=name=="screens_allowed"?(value==3?display[0]:value==1?display[1]:display[2]):value==0?timing[0]:value==30?timing[1]:timing[2];*static_cast<QString*>(a[0])=localized(*static_cast<QString*>(a[0]));}
 else {C(index==5);++writes;proxyWritten=*static_cast<unsigned*>(a[1]);fieldWritten=name;labelWritten=*static_cast<QString*>(a[3]);if(name=="screens_allowed")screens=display.indexOf(labelWritten)==0?3:display.indexOf(labelWritten)==1?1:2;else timeout=timing.indexOf(labelWritten)==1?30:300;}}
int main(int argc,char**argv){QCoreApplication app(argc,argv);ScreenFactoryProfile p;void*cam=&camera,*sys=&systemObject,*fmt=&formatterObject;p.proxy.slots[0]=&cam;p.proxy.slots[1]=&sys;
 p.proxy.meta[0]=p.proxy.meta[1]=&QObject::staticMetaObject;p.proxy.cast=[](const void*m,const void*o)->const void*{return static_cast<const QMetaObject*>(m)->cast(static_cast<const QObject*>(o));};p.proxy.call[0]=p.proxy.call[1]=proxyCall;
 p.proxy.fields[unsigned(FactoryField::EvfOnly)]={0,38,1,true,false,true};p.proxy.fields[unsigned(FactoryField::ScreensAllowed)]={1,33,4,false,false,true};p.proxy.fields[unsigned(FactoryField::ScreenTimeout)]={1,30,4,false,true,true};
 p.formatter.slot=&fmt;p.formatter.meta=&QObject::staticMetaObject;p.formatter.cast=p.proxy.cast;p.formatter.call=formatterCall;auto stock=qt_stock_profile();p.formatter.string_ctor=stock.string_ctor;p.formatter.string_deallocate=stock.string_deallocate;p.list_destroy=[](void*x){static_cast<QStringList*>(x)->~QStringList();};p.backBit=1;p.evfBit=2;p.systemProxy=4;p.cameraProxy=2;p.halfBit=2;
 ScreenFactorySnapshot s;C(screen_factory_snapshot(p,stock,ScreenControlKind::Display,true,s));C(s.state.count==3&&s.state.screensAllowed==1&&QString::fromUtf16(s.current)=="LCD");
 C(screen_factory_write(p,stock,ScreenControlKind::Display,{ScreenWriteKind::EvfOnly,1}));C(evf);C(screen_factory_write(p,stock,ScreenControlKind::Display,{ScreenWriteKind::ScreensAllowed,3}));C(screens==3);
 unsigned before=writes;C(!screen_factory_write(p,stock,ScreenControlKind::Display,{ScreenWriteKind::ScreensAllowed,7}));C(!screen_factory_write(p,stock,ScreenControlKind::Display,{ScreenWriteKind::DisplayStock,0,1,u"wrong"}));C(!screen_factory_write(p,stock,ScreenControlKind::Display,{ScreenWriteKind::DisplayStock,0,-1,u"LCD"}));C(writes==before);
 C(screen_factory_write(p,stock,ScreenControlKind::Display,{ScreenWriteKind::DisplayStock,0,2,u"EVF"}));C(proxyWritten==4&&fieldWritten=="screens_allowed"&&labelWritten=="EVF"&&screens==2);
 C(screen_factory_snapshot(p,stock,ScreenControlKind::Timeout,true,s));C(QString::fromUtf16(s.current)=="5 min"&&QString::fromUtf16(s.zero)=="off");C(screen_factory_write(p,stock,ScreenControlKind::Timeout,{ScreenWriteKind::TimeoutStock,0,1,u"30 sec"}));C(timeout==30);C(screen_factory_write(p,stock,ScreenControlKind::Timeout,{ScreenWriteKind::TimeoutZero,0}));C(timeout==0);
 C(screen_factory_snapshot(p,stock,ScreenControlKind::AdaptiveFps,true,s)&&!s.state.fps);C(screen_factory_write(p,stock,ScreenControlKind::AdaptiveFps,{ScreenWriteKind::AdaptiveFps,1}));C(fpsValue);C(!screen_factory_write(p,stock,ScreenControlKind::AdaptiveFps,{ScreenWriteKind::AdaptiveFps,2}));
 for(int i=0;i<3000;i++){C(screen_factory_snapshot(p,stock,ScreenControlKind::Display,true,s));C(display.size()==3&&display[2]=="EVF");} // 实际Qt隐式共享QStringList释放，不破坏原容器。
 chinese=true;screens=1;C(screen_factory_snapshot(p,stock,ScreenControlKind::Display,true,s));C(QString::fromUtf16(s.current)==QString::fromUtf16(u"仅显示屏"));C(QString::fromUtf16(s.labels[2])==QString::fromUtf16(u"仅取景器"));C(QString::fromUtf16(s.rawLabels[2])=="EVF");C(screen_factory_write(p,stock,ScreenControlKind::Display,{ScreenWriteKind::DisplayStock,0,2,u"仅取景器"}));C(labelWritten=="EVF"&&screens==2);C(!screen_factory_write(p,stock,ScreenControlKind::Display,{ScreenWriteKind::DisplayStock,0,2,u"EVF"}));timeout=300;C(screen_factory_snapshot(p,stock,ScreenControlKind::Timeout,true,s));C(QString::fromUtf16(s.current)==QString::fromUtf16(u"5分钟")&&QString::fromUtf16(s.zero)==QString::fromUtf16(u"关"));C(screen_factory_write(p,stock,ScreenControlKind::Timeout,{ScreenWriteKind::TimeoutStock,0,1,u"30秒"}));C(timeout==30&&labelWritten=="30 sec");chinese=false;
 display[1]=QString(128,'x');C(!screen_factory_snapshot(p,stock,ScreenControlKind::Display,true,s));display[1]="LCD";QStringList saved=display;display.clear();C(!screen_factory_snapshot(p,stock,ScreenControlKind::Display,true,s));display=saved;
 fmt=nullptr;C(!screen_factory_snapshot(p,stock,ScreenControlKind::Display,true,s));fmt=&formatterObject;cam=nullptr;C(!screen_factory_snapshot(p,stock,ScreenControlKind::AdaptiveFps,true,s));cam=&camera;
 printf("SCREEN_FACTORY_PASS assertions=%u repeated_actual_qt_list_release=3000 hardware_calls=0\n",checks);
}
