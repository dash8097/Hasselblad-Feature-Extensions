#ifndef HBL_TOUCH_BRIDGE_H
#define HBL_TOUCH_BRIDGE_H
#include "touch_core.h"
#include <QtQml/QQmlPropertyMap>
#include <QtQml/QJSValue>
#include <QtCore/QHash>
#include <QtCore/QVector>
class __attribute__((visibility("hidden"))) NativeTouchCore:public QQmlPropertyMap {
 QHash<QObject*,TouchCore> states;
public:
 explicit NativeTouchCore(QObject *parent):QQmlPropertyMap(parent){
  insert("request",QVariantList());insert("result",QVariantList());
  QObject::connect(this,&QQmlPropertyMap::valueChanged,[this](const QString &name,const QVariant &value){
   if(name!="request")return;
   const QVariantList a=value.userType()==qMetaTypeId<QJSValue>()?value.value<QJSValue>().toVariant().toList():value.toList();
   insert("request",QVariantList());insert("result",QVariantList()<<0<<0.0<<0.0<<0.0<<0.0);
   if(a.size()!=4)return;
   QObject *owner=a[0].value<QObject*>();const QVariantList frame=a[3].toList();
   if(!owner||frame.size()!=18)return;
   if(!states.contains(owner)){
    TouchCore init={};init.owner=-1;states.insert(owner,init);
    QObject::connect(owner,&QObject::destroyed,this,[this,owner](){states.remove(owner);});
   }
   TouchFrame f={};f.enabled=frame[0].toBool();f.focus=frame[1].toBool();f.zoom=frame[2].toBool();f.zoomActive=frame[3].toBool();
   f.left=frame[4].toDouble();f.top=frame[5].toDouble();f.padWidth=frame[6].toDouble();f.padHeight=frame[7].toDouble();
   f.width=frame[8].toDouble();f.height=frame[9].toDouble();f.xCount=frame[10].toDouble();f.yCount=frame[11].toDouble();
   f.marginX=frame[12].toDouble();f.marginY=frame[13].toDouble();f.itemWidth=frame[14].toDouble();f.itemHeight=frame[15].toDouble();
   f.currentX=frame[16].toDouble();f.currentY=frame[17].toDouble();
   const QVariantList input=a[2].toList();QVector<TouchPoint> points;
   if(input.size()>32)return;
   for(const QVariant &v:input){const QVariantList p=v.toList();if(p.size()!=3)return;TouchPoint t={p[0].toInt(),p[1].toDouble(),p[2].toDouble()};points.append(t);}
   const TouchResult r=touch_dispatch(&states[owner],a[1].toInt(),points.constData(),points.size(),&f);
   insert("result",QVariantList()<<int(r.effects)<<r.fx<<r.fy<<r.x<<r.y);
  });
 }
};
#endif
