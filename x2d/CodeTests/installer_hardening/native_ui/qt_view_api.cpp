#include "qt_view_api.hpp"
#include <QtQuick/private/qquickrectangle_p.h>
#include <QtQuick/private/qquicktext_p.h>
#include <QtQuick/private/qquickmousearea_p.h>
#include <QtQuick/private/qquickevents_p_p.h>
#include <QQmlParserStatus>
#include <memory>
namespace {
using namespace x2dui;
QQuickItem*item(void*p){return static_cast<QQuickItem*>(p);}
void*create(void*,Kind kind,void*parent){
 QQuickItem*p=item(parent),*result=nullptr;
 switch(kind){case Kind::Item:result=new QQuickItem(p);break;case Kind::Rectangle:result=new QQuickRectangle(p);break;case Kind::Text:result=new QQuickText(p);break;case Kind::Mouse:result=new QQuickMouseArea(p);break;}
 result->setParent(p);static_cast<QQmlParserStatus*>(result)->componentComplete();return result;
}
void destroy(void*,void*p){delete item(p);}
void geometry(void*,void*p,double x,double y,double w,double h){auto*i=item(p);i->setX(x);i->setY(y);i->setWidth(w);i->setHeight(h);}
void color(void*,void*p,unsigned rgb,bool text){auto c=QColor::fromRgb(rgb);if(text)static_cast<QQuickText*>(item(p))->setColor(c);else static_cast<QQuickRectangle*>(item(p))->setColor(c);}
void text(void*,void*p,const char16_t*str,double px,int align){auto*t=qobject_cast<QQuickText*>(item(p));if(!t)return;t->setText(QString::fromUtf16(str));QFont f=t->font();f.setPixelSize(int(px));t->setFont(f);t->setVAlign(QQuickText::AlignVCenter);t->setHAlign(align==1?QQuickText::AlignHCenter:align==2?QQuickText::AlignRight:QQuickText::AlignLeft);}
void visible(void*,void*p,bool v){item(p)->setVisible(v);}
void enabled(void*,void*p,bool v){item(p)->setEnabled(v);}
void opacity(void*,void*p,double v){item(p)->setOpacity(v);}
void name(void*,void*p,const char*s){item(p)->setObjectName(QString::fromUtf8(s));}
void layer(void*,void*p,double z,bool clip){item(p)->setZ(z);item(p)->setClip(clip);}
struct Listener {Handler handler;void*data;void(*release)(void*)=nullptr;Listener(Handler h,void*d):handler(h),data(d){}~Listener(){if(release)release(data);}void call(Event e,double n=0){handler(data,e,n);}};
bool listen(void*,void*p,Event event,Handler handler,void*data,void(*release)(void*)){
 // On failure ownership stays with core; only successful connects own Listener.
 if(!p||!handler||!release)return false;
 auto*object=item(p);auto*mouse=qobject_cast<QQuickMouseArea*>(object);
 if(event!=Event::Destroy&&event!=Event::Hidden&&!mouse)return false;
 auto l=std::make_shared<Listener>(handler,data);QMetaObject::Connection connection;
 switch(event){
 case Event::Destroy:connection=QObject::connect(object,&QObject::destroyed,object,[l](){l->call(Event::Destroy);},Qt::DirectConnection);break;
 case Event::Hidden:connection=QObject::connect(object,&QQuickItem::visibleChanged,object,[l,object](){if(!object->isVisible())l->call(Event::Hidden);});break;
 case Event::Click:connection=QObject::connect(mouse,&QQuickMouseArea::clicked,mouse,[l](QQuickMouseEvent*){l->call(Event::Click);});break;
 case Event::Press:connection=QObject::connect(mouse,static_cast<void(QQuickMouseArea::*)(QQuickMouseEvent*)>(&QQuickMouseArea::pressed),mouse,[l,mouse](QQuickMouseEvent*e){l->call(Event::Press,mouse->mapToScene({0,e->y()}).y());});break;
 case Event::Move:connection=QObject::connect(mouse,&QQuickMouseArea::positionChanged,mouse,[l,mouse](QQuickMouseEvent*e){l->call(Event::Move,mouse->mapToScene({0,e->y()}).y());});break;
 case Event::Release:connection=QObject::connect(mouse,&QQuickMouseArea::released,mouse,[l,mouse](QQuickMouseEvent*e){l->call(Event::Release,mouse->mapToScene({0,e->y()}).y());});break;
 default:return false;
 }
 if(!connection)return false;l->release=release;return true;
}
}
x2dui::ViewApi qt_view_api(){return {nullptr,create,destroy,geometry,color,text,visible,enabled,opacity,name,layer,listen};}
