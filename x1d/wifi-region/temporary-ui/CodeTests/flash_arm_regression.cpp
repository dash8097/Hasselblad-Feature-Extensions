// Standalone target-Qt differential test. No camera, D-Bus, file-provider or windows.
#include <QtCore/QCoreApplication>
#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QElapsedTimer>
#include <QtQml/QQmlEngine>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlComponent>
#include "../flash_logic.h"
#include <cstdio>
#include <unistd.h>

static QStringList warnings;
static QByteArray bytes(const QString &path) {QFile f(path);if(!f.open(QIODevice::ReadOnly))return QByteArray();return f.readAll();}
static void save(const QString &path,const QJsonObject &o){QFile f(path);if(f.open(QIODevice::WriteOnly))f.write(QJsonDocument(o).toJson());}
static QString firstDifference(const QJsonValue &a,const QJsonValue &b,const QString &path){
 if(a.type()!=b.type())return path+": type";
 if(a.isArray()){auto x=a.toArray(),y=b.toArray();if(x.size()!=y.size())return path+": length";for(int i=0;i<x.size();++i){auto r=firstDifference(x[i],y[i],path+"."+QString::number(i));if(!r.isEmpty())return r;}return QString();}
 if(a.isObject()){auto x=a.toObject(),y=b.toObject();if(x.keys()!=y.keys())return path+": keys";for(const auto &k:x.keys()){auto r=firstDifference(x[k],y[k],path+"."+k);if(!r.isEmpty())return r;}return QString();}
 return a==b?QString():path+": value";
}
static QJSValue checked(QJSValue v){if(v.isError())warnings.append(v.toString()+" "+v.property("stack").toString());return v;}
static QJSValue parse(QQmlEngine &e,const QJsonValue &v){QJsonArray a;a.append(v);return checked(e.evaluate(QString::fromUtf8(QJsonDocument(a).toJson(QJsonDocument::Compact))+"[0]"));}
static QJsonValue observation(QJSValue &context,const QJSValue &event){
 auto method=context.property("observe");auto value=checked(method.callWithInstance(context,QJSValueList()<<event));
 auto array=QJsonDocument::fromJson(value.toString().toUtf8()).array();return array.isEmpty()?QJsonValue():array[0];
}
static const char *factory=R"JS((function(page, specification, testCase) {
 var ctx={page:page, commands:[], counts:{}};
 function invoke(event) {
   if(event.op==='reset')return undefined;
   if(event.op==='set'){page[event.property]=event.value;return undefined;}
   return page[event.method].apply(page,event.args);
 }
 specification.signals.forEach(function(name){
   page[name].connect(function(){
     var signalArgs=Array.prototype.slice.call(arguments);
     ctx.commands.push(JSON.parse(JSON.stringify([name].concat(signalArgs))));
     ctx.counts[name]=(ctx.counts[name]||0)+1;
     (testCase.callbacks||[]).forEach(function(cb){
       if(cb.signal===name&&cb.occurrence===ctx.counts[name])cb.events.forEach(function(e){
         if(e.op==='signalAppend')signalArgs[e.argument].push(e.value);else invoke(e);
       });
     });
   });
 });
 ctx.observe=function(event){
   ctx.commands=[];
   var value=invoke(event),out={};
   if(typeof value==='undefined')value={$type:'undefined'};
   else if(typeof value==='number'&&!isFinite(value))value={$type:'number',value:String(value)};
   specification.stateFields.forEach(function(name){out[name]=page[name];});
   out.groups=[];
   for(var i=0;i<16;++i)out.groups.push([page.groupActive(i),page.groupPower(i)]);
   out.groupAdjustmentText=page.groupAdjustmentText();
   return JSON.stringify([{result:value,state:out,commands:ctx.commands}]);
 };
 return ctx;
}))JS";

int main(int argc,char **argv){
 alarm(60);
 QCoreApplication app(argc,argv);
 if(argc!=2)return 2;
 const QString dir=QString::fromLocal8Bit(argv[1]);
 const auto inputs=QJsonDocument::fromJson(bytes(dir+"/events.json")).object();
 const auto api=QJsonDocument::fromJson(bytes(dir+"/interface.json")).object();
 const auto cases=inputs["cases"].toArray();
 if(cases.isEmpty()||api.isEmpty())return 3;
 QElapsedTimer time;time.start();int records=0,assertions=0;
 QJsonObject result{{"passed",false},{"qtVersion",qVersion()},{"cameraRequests",0},{"cases",cases.size()}};
 auto fail=[&](const QString &why,const QString &caseName,int index,const QJsonValue &oldValue,const QJsonValue &newValue)->int{
   result["reason"]=why;result["case"]=caseName;result["index"]=index;result["records"]=records;
   result["elapsedMs"]=int(time.elapsed());result["warnings"]=QJsonArray::fromStringList(warnings);
   save(dir+"/result.json",result);save(dir+"/failure.json",QJsonObject{{"reason",why},{"case",caseName},{"index",index},{"baseline",oldValue},{"candidate",newValue}});
   std::printf("failed\n");return 1;
 };
 for(const auto &caseValue:cases){
   const auto testCase=caseValue.toObject();const QString name=testCase["name"].toString();
   QQmlEngine engine;
   QObject::connect(&engine,&QQmlEngine::warnings,&engine,[](const QList<QQmlError> &errors){for(const auto &e:errors)warnings.append(e.toString());});
   NativeFlashLogic bridge(&engine);engine.rootContext()->setContextProperty("_hblFlashCore",&bridge);
   QQmlComponent oldComponent(&engine),newComponent(&engine);
   oldComponent.setData(bytes(dir+"/baseline.qml"),QUrl::fromLocalFile(dir+"/baseline.qml"));
   newComponent.setData(bytes(dir+"/candidate.qml"),QUrl::fromLocalFile(dir+"/candidate.qml"));
   QObject *oldPage=oldComponent.create(),*newPage=newComponent.create();
   if(!oldPage||!newPage){for(const auto &e:oldComponent.errors())warnings.append(e.toString());for(const auto &e:newComponent.errors())warnings.append(e.toString());return fail("component",name,-1,QJsonValue(),QJsonValue());}
   QQmlEngine::setObjectOwnership(oldPage,QQmlEngine::CppOwnership);QQmlEngine::setObjectOwnership(newPage,QQmlEngine::CppOwnership);
   auto make=checked(engine.evaluate(QString::fromUtf8(factory)));
   auto oldContext=checked(make.call(QJSValueList()<<engine.newQObject(oldPage)<<parse(engine,api)<<parse(engine,testCase)));
   auto newContext=checked(make.call(QJSValueList()<<engine.newQObject(newPage)<<parse(engine,api)<<parse(engine,testCase)));
   auto events=testCase["events"].toArray();events.prepend(QJsonObject{{"op","reset"}});
   for(int i=0;i<events.size();++i){
     auto event=parse(engine,events[i]);const auto a=observation(oldContext,event),b=observation(newContext,event);
     ++records;
     if(!warnings.isEmpty())return fail("qml-warning",name,i-1,a,b);
     const auto diff=firstDifference(a,b,"observed");if(!diff.isEmpty())return fail(diff,name,i-1,a,b);
     const auto expected=events[i].toObject()["expect"].toObject();
     for(const auto &key:expected.keys()){
       QJsonValue actual=a;for(const auto &part:key.split('.'))actual=actual.isArray()?actual.toArray().at(part.toInt()):actual.toObject().value(part);
       const auto mismatch=firstDifference(expected[key],actual,key);if(!mismatch.isEmpty())return fail("baseline-assertion "+mismatch,name,i-1,a,b);
       ++assertions;
     }
   }
   delete newPage;delete oldPage;
 }
 result["passed"]=true;result["records"]=records;result["assertions"]=assertions;result["elapsedMs"]=int(time.elapsed());
 save(dir+"/result.json",result);std::printf("passed %d records\n",records);return 0;
}
