#ifndef HBL_FLASH_LOGIC_H
#define HBL_FLASH_LOGIC_H
#include <QtQml/QQmlPropertyMap>
#include <QtQml/QJSValue>
#include <QtQml/QQmlEngine>
#include <QtCore/QPointer>
#include <QtCore/QAbstractItemModel>
#include <QtCore/QMetaObject>
#include <QtCore/QStringList>
#include <algorithm>
#include <cmath>
#include <limits>

// UI-only rules. All external effects remain the existing page's signals.
// No device, transport, file or timer interfaces are available here.
namespace HblFlash {
static QVariant unpack(const QVariant &v) {
 return v.userType()==qMetaTypeId<QJSValue>()?v.value<QJSValue>().toVariant():v;
}
static bool number(const QVariant &v) {
 switch(v.type()) {case QVariant::Double:case QVariant::Int:case QVariant::UInt:
 case QVariant::LongLong:case QVariant::ULongLong:return true;default:return false;}
}
static bool integer(const QVariant &v) {return number(v)&&std::isfinite(v.toDouble())&&std::floor(v.toDouble())==v.toDouble();}
static bool truth(const QVariant &v) {
 if(!v.isValid()||v.isNull())return false;
 if(number(v)){const double n=v.toDouble();return n!=0&&!std::isnan(n);}
 if(v.type()==QVariant::String)return !v.toString().isEmpty();
 if(v.type()==QVariant::Bool)return v.toBool();
 return true;
}
static bool equal(const QVariant &a,const QVariant &b) {
 if(number(a)&&number(b))return a.toDouble()==b.toDouble();
 return a.type()==b.type()&&a==b;
}
static double nan() {return std::numeric_limits<double>::quiet_NaN();}
static double step(double value,const QVariant &direction,bool thirds,double low,double high) {
 if(!number(direction)||(direction.toDouble()!=-1&&direction.toDouble()!=1))return nan();
 const double d=direction.toDouble();
 if(!thirds)return value+d;
 if(d>0){for(int i=0;i<=24;++i){const double n=std::floor(i*10.0/3.0+0.5);if(n>value)return n;}return high+1;}
 for(int i=24;i>=0;--i){const double n=std::floor(i*10.0/3.0+0.5);if(n<value)return n;}
 return low-1;
}
static double clamp(double n,double low,double high) {return std::isnan(n)?n:std::max(low,std::min(high,n));}

struct Page {
 QPointer<QObject> object;
 QPointer<QAbstractItemModel> groups;
 int activeRole=-1,powerRole=-1;
 explicit Page(QObject *p):object(p) {
  if(!p)return;
  groups=qobject_cast<QAbstractItemModel*>(unpack(p->property("nativeGroups")).value<QObject*>());
  if(groups){const auto roles=groups->roleNames();for(auto i=roles.begin();i!=roles.end();++i){if(i.value()=="active")activeRole=i.key();if(i.value()=="tenthStops")powerRole=i.key();}}
 }
 QVariant get(const char *name)const{return object?unpack(object->property(name)):QVariant();}
 void set(const char *name,const QVariant &v){if(object)object->setProperty(name,v);}
 QVariant setArray(const char *name,const QVariantList &v){
  if(!object)return QVariant();
  QQmlEngine *engine=qmlEngine(object);
  if(!engine){set(name,v);return object?object->property(name):QVariant();}
  QJSValue array=engine->newArray(v.size());
  for(int i=0;i<v.size();++i)array.setProperty(i,engine->toScriptValue(v[i]));
  const QVariant value=QVariant::fromValue(array);set(name,value);return value;
 }
 bool boolean(const char *name)const{return get(name).toBool();}
 double real(const char *name)const{return get(name).toDouble();}
 int count()const{return groups?groups->rowCount():0;}
 bool index(const QVariant &i,int limit)const{return integer(i)&&i.toDouble()>=0&&i.toDouble()<limit;}
 QVariant row(int i,int role)const{return groups&&i>=0&&i<count()?groups->data(groups->index(i,0),role):QVariant();}
 bool rowSet(int i,int role,const QVariant &v){
  if(!groups||i<0||i>=count())return false;
  // Qt 5.5 QQmlListModel does not implement QAbstractItemModel::setData.
  // Invoke the same public slot as QML's ListModel.setProperty to keep its notifications.
  const QString name=role==activeRole?QStringLiteral("active"):QStringLiteral("tenthStops");
  return QMetaObject::invokeMethod(groups,"setProperty",Qt::DirectConnection,Q_ARG(int,i),Q_ARG(QString,name),Q_ARG(QVariant,v));
 }
 QVariantList visible()const{return get("visibleGroups").toList();}
 bool syncAllowed()const{return boolean("connected")&&boolean("masterEnabled")&&!boolean("busy")&&boolean("sendFlashSync");}
 bool powerAllowed()const{return boolean("connected")&&boolean("masterEnabled")&&!boolean("busy")&&boolean("sendPowerUpdates");}
 void signal0(const char *s){if(object)QMetaObject::invokeMethod(object,s,Qt::DirectConnection);}
 void signalB(const char *s,bool a){if(object)QMetaObject::invokeMethod(object,s,Qt::DirectConnection,Q_ARG(bool,a));}
 void signalBB(const char *s,bool a,bool b){if(object)QMetaObject::invokeMethod(object,s,Qt::DirectConnection,Q_ARG(bool,a),Q_ARG(bool,b));}
 void signalIB(const char *s,int a,bool b){if(object)QMetaObject::invokeMethod(object,s,Qt::DirectConnection,Q_ARG(int,a),Q_ARG(bool,b));}
 void signalII(const char *s,int a,int b){if(object)QMetaObject::invokeMethod(object,s,Qt::DirectConnection,Q_ARG(int,a),Q_ARG(int,b));}
 void signalIBI(const char *s,int a,bool b,int c){if(object)QMetaObject::invokeMethod(object,s,Qt::DirectConnection,Q_ARG(int,a),Q_ARG(bool,b),Q_ARG(int,c));}
 void signalV(const char *s,const QVariant &v){if(object)QMetaObject::invokeMethod(object,s,Qt::DirectConnection,Q_ARG(QVariant,v));}
 double stepped(double value,const QVariant &direction)const{return step(value,direction,boolean("thirdStopSteps"),real("minimumPower"),real("maximumPower"));}
 double bounded(double value)const{return clamp(value,real("minimumPower"),real("maximumPower"));}
 bool powerUpdate(const QVariant &i){
  if(!powerAllowed()||!index(i,count()))return false;
  const int n=i.toInt();signalIBI("powerUpdateRequested",n,row(n,activeRole).toBool(),row(n,powerRole).toInt());return true;
 }
 bool setGroup(const QVariant &i,const QVariant &active,const QVariant &value,const QVariant &draft){
  if(!index(i,get("supportedGroupCount").toInt())||!integer(value)||value.toDouble()<real("minimumPower")||value.toDouble()>real("maximumPower")||!index(i,count()))return false;
  const int n=i.toInt();if(equal(row(n,activeRole),active)&&equal(row(n,powerRole),value))return true;
  if(!boolean("applyingGroupAdjustment"))set("groupAdjustmentSteps",0);
  if(!rowSet(n,activeRole,active)||!rowSet(n,powerRole,value))return false;
  if(truth(draft)){
   set("hasDraftChanges",true);signalIBI("groupDraftChanged",n,truth(active),value.toInt());
   // Read the current gate after the synchronous draft callback, as the old UI did.
   powerUpdate(i);
  }
  return true;
 }
 bool canAdjust(const QVariant &direction)const{
  const auto list=visible();if(list.isEmpty()||!number(direction)||(direction.toDouble()!=-1&&direction.toDouble()!=1))return false;
  for(const auto &i:list){if(!index(i,count()))return false;const double n=stepped(row(i.toInt(),powerRole).toDouble(),direction);if(n<real("minimumPower")||n>real("maximumPower"))return false;}
  return true;
 }
 bool wirelessValid()const{
  if(get("wirelessField").toString()=="id"&&boolean("wirelessOff"))return true;
  const QString input=get("wirelessInput").toString();if(input.isEmpty()||input.size()>2)return false;
  for(const auto c:input)if(c<'0'||c>'9')return false;
  const int n=input.toInt();return n>=1&&n<=(get("wirelessField").toString()=="channel"?32:99);
 }
 QVariant run(int op,const QVariantList &a){
  auto arg=[&a](int i)->QVariant{return i<a.size()?unpack(a[i]):QVariant();};
  if(!object||!groups||activeRole<0||powerRole<0)return false;
  switch(op){
   case 2:return setGroup(arg(0),arg(1),arg(2),arg(3));
   case 3:{const auto sync=arg(1);if(sync.type()!=QVariant::Bool)return false;if(boolean("sendPowerUpdates")&&boolean("sendFlashSync")==sync.toBool())return true;set("sendPowerUpdates",true);set("sendFlashSync",sync);signalBB("deliveryOptionsChanged",true,sync.toBool());return true;}
   case 4:return powerUpdate(arg(0));
   case 5:if(!syncAllowed())return false;signal0("flashSyncRequested");return true;
   case 6:{if(!syncAllowed()||!boolean("canTest"))return false;bool active=false;for(int i=0;i<count();++i)if(row(i,activeRole).toBool()){active=true;break;}if(!active)return false;signal0("testRequested");return true;}
   case 7:{const auto thirds=arg(0);if(thirds.type()!=QVariant::Bool)return false;if(boolean("thirdStopSteps")==thirds.toBool())return true;set("thirdStopSteps",thirds);set("groupAdjustmentSteps",0);if(truth(arg(1)))signalB("adjustmentStepRequested",thirds.toBool());return true;}
   case 8:return stepped(arg(0).toDouble(),arg(1));
   case 9:{const auto i=get("selectedGroup");if(!index(i,count()))return false;return setGroup(i,row(i.toInt(),activeRole),bounded(stepped(row(i.toInt(),powerRole).toDouble(),arg(0))),true);}
   case 10:{const auto i=arg(0);if(!index(i,count())||!row(i.toInt(),activeRole).toBool())return false;return setGroup(i,true,bounded(stepped(row(i.toInt(),powerRole).toDouble(),arg(1))),true);}
   case 11:{const auto i=arg(0),steps=arg(2);if(!index(i,count())||!integer(steps)||!number(arg(1))||!row(i.toInt(),activeRole).toBool())return false;double next=arg(1).toDouble();const int direction=steps.toDouble()<0?-1:1;for(int n=0;n<std::min(std::abs(steps.toDouble()),80.0);++n)next=bounded(stepped(next,direction));return setGroup(i,true,next,true);}
   case 14:{const auto i=arg(0);if(!index(i,count()))return false;return setGroup(i,!row(i.toInt(),activeRole).toBool(),row(i.toInt(),powerRole),true);}
   case 15:{const auto i=arg(0);if(!boolean("modelingLampAvailable")||!index(i,get("supportedGroupCount").toInt())||!index(i,count()))return false;auto lamps=get("lampStates").toList();if(i.toInt()>=lamps.size())return false;const bool next=!truth(lamps[i.toInt()]);lamps[i.toInt()]=next;set("lampStates",lamps);set("hasDraftChanges",true);signalIB("modelingLampDraftChanged",i.toInt(),next);return true;}
   case 16:return canAdjust(arg(0));
   case 17:{if(!canAdjust(arg(0)))return false;set("applyingGroupAdjustment",true);for(int n=0;object&&n<visible().size();++n){const auto i=visible()[n];setGroup(i,row(i.toInt(),activeRole),stepped(row(i.toInt(),powerRole).toDouble(),arg(0)),true);}set("applyingGroupAdjustment",false);set("groupAdjustmentSteps",get("groupAdjustmentSteps").toInt()+arg(0).toInt());return true;}
   case 20:{const auto i=arg(0);if(!index(i,get("supportedGroupCount").toInt())||!index(i,count()))return false;auto list=visible();int at=-1;for(int n=0;n<list.size();++n)if(equal(list[n],i)){at=n;break;}if(at>=0)list.removeAt(at);else list.append(i);std::sort(list.begin(),list.end(),[](const QVariant &a,const QVariant &b){return a.toDouble()<b.toDouble();});const auto array=setArray("visibleGroups",list);signalV("visibleGroupsRequested",array);set("groupAdjustmentSteps",0);QObject *view=get("nativeList").value<QObject*>();if(view)view->setProperty("contentY",0);return true;}
   case 22:{const QString field=arg(0).toString();if(arg(0).type()!=QVariant::String||(field!="channel"&&field!="id"))return false;set("wirelessField",field);set("wirelessInput",QString::number(get(field=="channel"?"channel":"wirelessId").toInt()));set("wirelessOff",field=="id"&&get("wirelessId").toInt()==0);set("wirelessInputFresh",true);set("screen","wireless");return true;}
   case 23:{if(get("screen").toString()!="wireless")return false;const QString key=arg(0).toString();if(key==QString::fromUtf8("清除")){set("wirelessInput","");set("wirelessOff",false);set("wirelessInputFresh",false);return true;}if(key==QString::fromUtf8("退格")){QString input=get("wirelessInput").toString();input.chop(1);set("wirelessInput",boolean("wirelessOff")?QString():input);set("wirelessOff",false);set("wirelessInputFresh",false);return true;}if(key.size()!=1||key[0]<'0'||key[0]>'9')return false;if(boolean("wirelessInputFresh")||boolean("wirelessOff"))set("wirelessInput","");set("wirelessInputFresh",false);set("wirelessOff",false);QString input=get("wirelessInput").toString();if(input.size()>=2)return false;set("wirelessInput",input+key);return true;}
   case 24:{if(get("screen").toString()!="wireless"||!wirelessValid())return false;const int value=boolean("wirelessOff")?0:get("wirelessInput").toString().toInt();const QString field=get("wirelessField").toString();signalII("wirelessConfigurationRequested",field=="channel"?value:get("channel").toInt(),field=="id"?value:get("wirelessId").toInt());set("screen","groups");return true;}
   default:return false;
  }
 }
};
}

class __attribute__((visibility("hidden"))) NativeFlashLogic:public QQmlPropertyMap {
public:
 explicit NativeFlashLogic(QObject *parent):QQmlPropertyMap(parent){
  insert("request",QVariantList());insert("result",false);
  QObject::connect(this,&QQmlPropertyMap::valueChanged,this,[this](const QString &name,const QVariant &v){
   if(name!="request")return;
   const auto input=HblFlash::unpack(v).toList();insert("request",QVariantList());
   QVariant result=false;
   if(input.size()==3&&HblFlash::integer(input[1])){
    HblFlash::Page page(HblFlash::unpack(input[0]).value<QObject*>());
    result=page.run(input[1].toInt(),HblFlash::unpack(input[2]).toList());
   }
   // Calls made by a binding must not subscribe every caller to a shared result.
   const bool blocked=blockSignals(true);insert("result",result);blockSignals(blocked);
  });
 }
};
#endif
