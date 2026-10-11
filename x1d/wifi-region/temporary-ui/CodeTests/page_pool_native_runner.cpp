// Offline UI fixture for target Qt 5.5.1. No camera/transport API and no window.
#include <QtCore/QCoreApplication>
#include <QtCore/QElapsedTimer>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QThread>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlIncubator>
#include "../page_pool_core.h"
#include <cstdio>
#include <unistd.h>

static QStringList warnings;
static QStringList allowedWarnings;
static QByteArray read(const QString &path){QFile file(path);return file.open(QIODevice::ReadOnly)?file.readAll():QByteArray();}
static void save(const QString &path,const QJsonObject &data){QFile file(path);if(file.open(QIODevice::WriteOnly))file.write(QJsonDocument(data).toJson());}
static QString diff(const QJsonValue &a,const QJsonValue &b,const QString &path){
    if(a.type()!=b.type())return path+": type";
    if(a.isArray()){auto x=a.toArray(),y=b.toArray();if(x.size()!=y.size())return path+": length";for(int i=0;i<x.size();++i){const auto d=diff(x[i],y[i],path+"."+QString::number(i));if(!d.isEmpty())return d;}return QString();}
    if(a.isObject()){auto x=a.toObject(),y=b.toObject();if(x.keys()!=y.keys())return path+": keys";for(const auto &k:x.keys()){const auto d=diff(x[k],y[k],path+"."+k);if(!d.isEmpty())return d;}return QString();}
    return a==b?QString():path+": value";
}
static QJsonValue at(QJsonValue value,const QString &path){for(const auto &key:path.split('.'))value=value.isArray()?value.toArray().at(key.toInt()):value.toObject().value(key);return value;}
static QJSValue invoke(QQmlEngine &engine,QObject *object,const char *method,const QJsonObject *event=nullptr){
    auto self=engine.newQObject(object),fn=self.property(method);QJSValueList args;
    if(event)args.append(engine.toScriptValue(event->toVariantMap()));
    auto result=fn.callWithInstance(self,args);
    if(result.isError())warnings.append(result.toString()+" "+result.property("stack").toString());
    return result;
}
static QJsonValue observe(QQmlEngine &engine,QObject *object){return QJsonDocument::fromJson(invoke(engine,object,"observe").toString().toUtf8()).object();}
static void settle(QQmlIncubationController &controller){
    QElapsedTimer elapsed;elapsed.start();
    QCoreApplication::processEvents();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
    // Deliberately do not incubate between non-settle events. This makes
    // selection/hide/destruction before asynchronous readiness deterministic.
    while(elapsed.elapsed()<100||controller.incubatingObjectCount()) {
        controller.incubateFor(3);QCoreApplication::processEvents();
        QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        QThread::msleep(1);if(elapsed.elapsed()>3000){warnings.append("fixture incubation timeout");break;}
    }
}
int main(int argc,char **argv){
    alarm(90);QCoreApplication app(argc,argv);if(argc!=2)return 2;
    const QString dir=QString::fromLocal8Bit(argv[1]);
    const auto cases=QJsonDocument::fromJson(read(dir+"/events.json")).object()["cases"].toArray();if(cases.isEmpty())return 3;
    QElapsedTimer clock;clock.start();int records=0,comparisons=0,assertions=0,repairs=0,destructions=0;
    QJsonObject result{{"passed",false},{"qtVersion",qVersion()},{"cameraRequests",0},{"cases",cases.size()}};
    QFile trace(dir+"/trace.jsonl");trace.open(QIODevice::WriteOnly);
    auto fail=[&](const QString &why,const QString &name,int index,const QJsonValue &a,const QJsonValue &b)->int{
        result["reason"]=why;result["case"]=name;result["event"]=index;result["records"]=records;result["warnings"]=QJsonArray::fromStringList(warnings);result["allowlistedWarnings"]=QJsonArray::fromStringList(allowedWarnings);
        save(dir+"/result.json",result);save(dir+"/failure.json",QJsonObject{{"reason",why},{"case",name},{"event",index},{"baseline",a},{"candidate",b}});
        std::printf("failed: %s / %s\n",name.toUtf8().constData(),why.toUtf8().constData());return 1;
    };
    for(const auto &caseValue:cases){
        const auto testCase=caseValue.toObject();const QString name=testCase["name"].toString();
        const bool repair=testCase["mode"].toString()=="repair";bool differed=false;
        QQmlEngine engine;QQmlIncubationController controller;engine.setIncubationController(&controller);int currentEvent=-1;
        QObject::connect(&engine,&QQmlEngine::warnings,&engine,[&name,&currentEvent](const QList<QQmlError> &errors){for(const auto &error:errors){
            const auto text=error.toString();const auto file=error.url().fileName();
            const bool fixtureFailure=name=="source-failure-unlocks-next-page"&&currentEvent==1;
            const bool missing=fixtureFailure&&file=="Missing.qml";
            const bool oldStatusLoop=fixtureFailure&&error.line()==71&&error.column()==9&&
                ((file=="BaselineHarness.qml"&&text.endsWith(": QML BaselinePool: Binding loop detected for property \"status\""))||
                 (file=="CandidateHarness.qml"&&text.endsWith(": QML CandidatePool: Binding loop detected for property \"status\"")));
            if(missing||oldStatusLoop)allowedWarnings.append(text);else warnings.append(text);
        }});
        NativePagePoolCore bridge(&engine);engine.rootContext()->setContextProperty("_hblPagePoolCore",&bridge);
        QQmlComponent oldComponent(&engine),newComponent(&engine);
        oldComponent.setData(read(dir+"/BaselineHarness.qml"),QUrl::fromLocalFile(dir+"/BaselineHarness.qml"));
        newComponent.setData(read(dir+"/CandidateHarness.qml"),QUrl::fromLocalFile(dir+"/CandidateHarness.qml"));
        QObject *oldHost=oldComponent.create(),*newHost=newComponent.create();
        if(!oldHost||!newHost){for(const auto &e:oldComponent.errors())warnings.append(e.toString());for(const auto &e:newComponent.errors())warnings.append(e.toString());return fail("create",name,-1,QJsonValue(),QJsonValue());}
        QQmlEngine::setObjectOwnership(oldHost,QQmlEngine::CppOwnership);QQmlEngine::setObjectOwnership(newHost,QQmlEngine::CppOwnership);
        const auto events=testCase["events"].toArray();
        for(int index=0;index<events.size();++index){
            currentEvent=index;const auto event=events[index].toObject();invoke(engine,oldHost,"run",&event);invoke(engine,newHost,"run",&event);
            if(event["op"].toString()=="settle")settle(controller);
            const auto a=observe(engine,oldHost),b=observe(engine,newHost);++records;
            trace.write(QJsonDocument(QJsonObject{{"case",name},{"index",index},{"baseline",a},{"candidate",b}}).toJson(QJsonDocument::Compact)+"\n");trace.flush();
            if(!warnings.isEmpty())return fail("qml-warning",name,index,a,b);
            const auto mismatch=diff(a,b,"state");if(!mismatch.isEmpty())differed=true;
            if(!repair){++comparisons;if(!mismatch.isEmpty())return fail(mismatch,name,index,a,b);}
            for(int variant=0;variant<2;++variant){
                const auto snapshot=variant?b:a;
                auto expected=event["expect"].toObject();const auto specific=event[variant?"candidate":"baseline"].toObject();
                for(const auto &key:specific.keys())expected[key]=specific[key];
                for(const auto &key:expected.keys()){
                    const auto error=diff(expected[key],at(snapshot,key),key);++assertions;
                    if(!error.isEmpty())return fail(QString(variant?"candidate ":"baseline ")+error,name,index,a,b);
                }
            }
        }
        if(repair){++repairs;if(!differed)return fail("repair did not expose baseline defect",name,-1,QJsonValue(),QJsonValue());}
        delete newHost;delete oldHost;settle(controller);
        if(bridge.poolCount()!=0)return fail("native pool leaked after host destruction",name,-1,QJsonValue(),QJsonValue());
        if(!warnings.isEmpty())return fail("qml-warning during destruction",name,-1,QJsonValue(),QJsonValue());
        ++destructions;
    }
    result["passed"]=true;result["records"]=records;result["strictComparisons"]=comparisons;result["explicitAssertions"]=assertions;
    result["repairCases"]=repairs;result["hostCleanupChecks"]=destructions;result["elapsedMs"]=int(clock.elapsed());result["allowlistedWarnings"]=QJsonArray::fromStringList(allowedWarnings);
    save(dir+"/result.json",result);std::printf("passed %d records, %d strict comparisons, %d assertions, %d explicit repairs\n",records,comparisons,assertions,repairs);return 0;
}
