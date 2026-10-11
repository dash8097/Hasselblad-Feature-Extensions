// Offline, headless Qt5.5.1 differential runner. No camera interfaces or windows.
#include <QtCore/QCoreApplication>
#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QElapsedTimer>
#include <QtCore/QTranslator>
#include <QtQml/QQmlEngine>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlComponent>
#include <QtQml/qqml.h>
#include "../settings_rules_core.h"
#include <cstdio>
#include <csignal>
#include <unistd.h>

static char currentEvent[96]={0};
static int currentEventLength=0;
static void checkpoint(int caseIndex,int eventIndex,const char *phase) {
    const int n=std::snprintf(currentEvent,sizeof(currentEvent),"event %d %d %s\n",caseIndex,eventIndex,phase);
    currentEventLength=qMax(0,qMin(n,int(sizeof(currentEvent)-1)));
}
static void crash(int signal) {
    // Fixed, bounded diagnostic only. No Qt, allocation or unsafe stdio here.
    ::write(1,"fatal-signal\n",13);::write(1,currentEvent,currentEventLength);
#ifdef HBL_SETTINGS_RULES_TRACE
    const unsigned end=HblSettingsRules::traceSequence,start=end>32?end-32:0;
    for(unsigned i=start;i<end;++i){const unsigned slot=i%32;::write(1,HblSettingsRules::traceRing[slot],HblSettingsRules::traceLengths[slot]);}
#endif
    _exit(128+signal);
}

static QByteArray bytes(const QString &path){QFile f(path);return f.open(QIODevice::ReadOnly)?f.readAll():QByteArray();}
static void save(const QString &path,const QJsonObject &value){QFile f(path);if(f.open(QIODevice::WriteOnly))f.write(QJsonDocument(value).toJson());}
static QString singletonFixtureDirectory;
static QObject *settingsSingleton(QQmlEngine *engine,QJSEngine *) {
    // A registered Qt QObject singleton with only the fixture's simulated
    // methods. Never registers/imports the firmware Settings service.
    QQmlComponent component(engine);
    const QString path=singletonFixtureDirectory+"/settings-api.qml";
    component.setData(bytes(path),QUrl::fromLocalFile(path));
    QObject *instance=component.create();
    engine->setProperty("offlineSettingsApi",QVariant::fromValue(instance));
    return instance;
}
static QString difference(const QJsonValue &a,const QJsonValue &b,const QString &path){
    if(a.type()!=b.type())return path+": type";
    if(a.isArray()){const auto x=a.toArray(),y=b.toArray();if(x.size()!=y.size())return path+": length";for(int i=0;i<x.size();++i){const auto d=difference(x[i],y[i],path+"."+QString::number(i));if(!d.isEmpty())return d;}return QString();}
    if(a.isObject()){const auto x=a.toObject(),y=b.toObject();if(x.keys()!=y.keys())return path+": keys";for(const auto &k:x.keys()){const auto d=difference(x[k],y[k],path+"."+k);if(!d.isEmpty())return d;}return QString();}
    return a==b?QString():path+": value";
}
class Proxy:public QQmlPropertyMap {
public:
    QJsonArray writes;
    explicit Proxy(QObject *parent):QQmlPropertyMap(parent){}
protected:
    QVariant updateValue(const QString &name,const QVariant &input) override {
        const QVariant value=HblSettingsRules::unpack(input);
        QJsonArray record;record.append(name);record.append(QJsonValue::fromVariant(value));writes.append(record);
        if(this->value("deferWrites").toBool()&&(name=="brightness"||name=="fine"||name=="zeroStep"))return this->value(name);
        return value;
    }
};
struct Fixture {
    QQmlEngine engine;
    NativeSettingsRules bridge;
    Proxy store;
    QQmlPropertyMap radio,config;
    QQmlComponent component;
    QPointer<QObject> root;
    QJSValue page;
    QStringList warnings;
    int eventsObserved=0;
    Fixture(const QString &path,const QJsonObject &inputs,bool withoutNative,bool singleton=false):bridge(&engine),store(&engine),radio(&engine),config(&engine),component(&engine) {
        QObject::connect(&engine,&QQmlEngine::warnings,&engine,[this](const QList<QQmlError> &errors){for(const auto &e:errors)warnings.append(e.toString());});
        for(const auto &key:inputs["store"].toObject().keys())store.insert(key,inputs["store"].toObject()[key].toVariant());
        for(const auto &key:inputs["radio"].toObject().keys())radio.insert(key,inputs["radio"].toObject()[key].toVariant());
        config.insert("hideDemoItems",true);
        engine.rootContext()->setContextProperty("configstore",&store);
        engine.rootContext()->setContextProperty("guiconfig",&config);
        if(!withoutNative)engine.rootContext()->setContextProperty("hblNative",&radio);
        engine.rootContext()->setContextProperty("_hblSettingsRulesCore",&bridge);
        QByteArray source=bytes(path);
        if(singleton) {
            source.prepend("import HblSettingsApiFixture 1.0\n");
            source.replace("settings.","Settings.");
            source.replace("nativeSettingsApi:settings","nativeSettingsApi:Settings");
        }
        component.setData(source,QUrl::fromLocalFile(path));
        root=component.create();
        if(!root){for(const auto &e:component.errors())warnings.append(e.toString());return;}
        QQmlEngine::setObjectOwnership(root,QQmlEngine::CppOwnership);
        page=engine.newQObject(root);
    }
    ~Fixture(){delete root;}
    QJsonObject bridgeDiagnostics() {
        QJSValue json=engine.globalObject().property("JSON"),stringify=json.property("stringify");
        QJSValue result=stringify.isCallable()?stringify.callWithInstance(json,QJSValueList()<<engine.newArray()):QJSValue();
        HblSettingsRules::Page adapter(root);
        QJsonObject out{{"globalJsonIsObject",json.isObject()},{"globalStringifyCallable",stringify.isCallable()},
            {"globalStringifyArrayResult",result.toString()},{"globalStringifyIsError",result.isError()},
            {"bridgeResult",QJsonValue::fromVariant(bridge.value("result"))},
            {"nativePageIsQObject",adapter.get("nativePage").isQObject()}};
        if(root) {
            const QVariant original=root->property("lastRows");
            out["qjsStringPropertyAccepted"]=root->setProperty("lastRows",QVariant::fromValue(QJSValue(QStringLiteral("probe"))));
            out["qjsStringPropertyReadback"]=root->property("lastRows").toString();
            root->setProperty("lastRows",original);
        }
        return out;
    }
    QJsonObject observe(const QJsonObject &event) {
        if(!root)return QJsonObject();
        page.setProperty("commands",engine.newArray());store.writes=QJsonArray();
        const QString operation=event["op"].toString();
        if(operation=="set") {
            const QString target=event["target"].toString(),name=event["name"].toString();
            const QVariant value=event["value"].toVariant();
            if(target=="root")root->setProperty(name.toUtf8().constData(),value);
            else if(target=="store")store.insert(name,value);
            else if(target=="radio")radio.insert(name,value);
            else if(target=="config")config.insert(name,value);
            else warnings.append("Unknown test target");
        }else if(operation=="call") {
            QJSValue method=page.property(event["method"].toString());QJSValueList args;
            for(const auto &value:event["args"].toArray())args.append(engine.toScriptValue(value.toVariant()));
            const auto result=method.callWithInstance(page,args);
            if(result.isError())warnings.append(result.toString()+" "+result.property("stack").toString());
        }
        QCoreApplication::processEvents();
        if(eventsObserved++%17==0)engine.collectGarbage();
        QJSValue method=page.property("testSnapshot");const auto result=method.callWithInstance(page);
        if(result.isError())warnings.append(result.toString()+" "+result.property("stack").toString());
        auto value=QJsonDocument::fromJson(result.toString().toUtf8()).object();value["writes"]=store.writes;
        return value;
    }
};
static QJsonObject singletonRegression(const QString &dir,const QJsonObject &inputs) {
    Fixture baseline(dir+"/baseline.qml",inputs,false,true),candidate(dir+"/candidate.qml",inputs,false,true);
    QJsonObject report{{"passed",false},{"registeredQObjectSingleton",true},{"records",0},{"forcedGcEachEvent",true}};
    const QJSValue wrapper=HblSettingsRules::Page(candidate.root).get("nativeSettingsApi");
    report["wrapperIsObject"]=wrapper.isObject();report["wrapperIsQObject"]=wrapper.isQObject();
    report["methodIsCallable"]=wrapper.property("getDisplayValue").isCallable();
    QObject *oldApi=baseline.engine.property("offlineSettingsApi").value<QObject*>(),*newApi=candidate.engine.property("offlineSettingsApi").value<QObject*>();
    auto calls=[](QObject *api)->QJsonValue{return api?QJsonValue::fromVariant(HblSettingsRules::unpack(api->property("apiCalls"))):QJsonValue();};
    QString reason;
    if(!baseline.root||!candidate.root||!oldApi||!newApi||!wrapper.isObject()||!wrapper.property("getDisplayValue").isCallable())reason="singleton/component";
    if(reason.isEmpty())reason=difference(calls(oldApi),calls(newApi),"singleton-creation-calls");
    const QJsonArray events{
        QJsonObject{{"op","reset"}},
        QJsonObject{{"op","call"},{"method","rebuild"},{"args",QJsonArray()}},
        QJsonObject{{"op","set"},{"target","store"},{"name","testValue"},{"value",3}},
        QJsonObject{{"op","call"},{"method","rebuild"},{"args",QJsonArray()}},
        QJsonObject{{"op","call"},{"method","testValue"},{"args",QJsonArray{"brightness",7.6}}},
        QJsonObject{{"op","set"},{"target","store"},{"name","languageIndex"},{"value",1}},
        QJsonObject{{"op","set"},{"target","root"},{"name","itemValues"},{"value","cameraSettingsAutofocus"}},
        QJsonObject{{"op","set"},{"target","store"},{"name","languageIndex"},{"value",2}},
        QJsonObject{{"op","call"},{"method","rebuild"},{"args",QJsonArray()}}
    };
    int records=0;
    for(const auto &entry:events) {
        if(!reason.isEmpty())break;
        oldApi->setProperty("apiCalls",QVariant::fromValue(baseline.engine.newArray()));
        newApi->setProperty("apiCalls",QVariant::fromValue(candidate.engine.newArray()));
        baseline.engine.collectGarbage();candidate.engine.collectGarbage();
        const auto before=baseline.observe(entry.toObject()),after=candidate.observe(entry.toObject());++records;
        reason=difference(before,after,"singleton-state");
        if(reason.isEmpty())reason=difference(calls(oldApi),calls(newApi),"singleton-calls");
    }
    if(reason.isEmpty()&&(!baseline.warnings.isEmpty()||!candidate.warnings.isEmpty()))reason="qml-warning";
    report["records"]=records;report["passed"]=reason.isEmpty();
    if(!reason.isEmpty()){report["reason"]=reason;report["warnings"]=QJsonArray::fromStringList(baseline.warnings+candidate.warnings);}
    return report;
}
class FixtureTranslator:public QTranslator {
public:
    mutable QJsonArray requests;
    int language=0;
    bool isEmpty()const override{return false;}
    QString translate(const char *context,const char *source,const char *comment,int n)const override {
        if(QByteArray(context)!="MENUS")return QString();
        requests.append(QJsonObject{{"context",QString::fromUtf8(context)},
            {"source",QString::fromUtf8(source)},{"comment",comment?QJsonValue(QString::fromUtf8(comment)):QJsonValue(QJsonValue::Null)}, {"n",n}});
        const QString suffix=language?QString::fromUtf8(" β"):QString::fromUtf8(" α");
        const QByteArray text(source);
        if(text=="L0:Heading"||text=="L1:Heading")return QString::fromUtf8("标题")+suffix;
        if(text=="L0:Flag"||text=="L1:Flag")return QString::fromUtf8("开关")+suffix;
        if(text=="Auto Focus")return QString::fromUtf8("自动对焦")+suffix;
        if(text=="Manual Focus")return QString::fromUtf8("手动对焦")+suffix;
        return QString(); // Real QCoreApplication untranslated-source fallback.
    }
};
static QJsonObject translationRegression(const QString &dir,const QJsonObject &inputs) {
    FixtureTranslator translator;
    const bool installed=QCoreApplication::installTranslator(&translator);
    Fixture baseline(dir+"/baseline.qml",inputs,false);
    const QJsonArray baselineCreation=translator.requests;translator.requests=QJsonArray();
    Fixture candidate(dir+"/candidate.qml",inputs,false);
    const QJsonArray candidateCreation=translator.requests;translator.requests=QJsonArray();
    QJsonObject report{{"passed",false},{"realQTranslator",true},{"records",0},{"assertions",0}};
    QString reason;
    if(!installed||!baseline.root||!candidate.root)reason="translator/component";
    if(reason.isEmpty())reason=difference(baselineCreation,candidateCreation,"creation-translation-calls");
    int records=0,assertions=0;
    auto check=[&](const QJsonObject &event,const QJsonObject &expected) {
        if(!reason.isEmpty())return;
        translator.requests=QJsonArray();const auto before=baseline.observe(event);const QJsonArray oldCalls=translator.requests;
        translator.requests=QJsonArray();const auto after=candidate.observe(event);const QJsonArray newCalls=translator.requests;
        ++records;
        reason=difference(before,after,"translation-state");
        if(reason.isEmpty())reason=difference(oldCalls,newCalls,"translation-calls");
        for(const auto &key:expected.keys()) {
            QJsonValue actual=before;for(const auto &part:key.split('.'))actual=actual.isArray()?actual.toArray().at(part.toInt()):actual.toObject().value(part);
            if(reason.isEmpty())reason=difference(expected[key],actual,key);++assertions;
        }
    };
    check(QJsonObject{{"op","reset"}},QJsonObject{{"rows.0.label",QString::fromUtf8("标题 α")},{"rows.1.label",QString::fromUtf8("开关 α")},{"rows.2.label","L0:Four"}});
    check(QJsonObject{{"op","set"},{"target","root"},{"name","itemValues"},{"value","cameraSettingsAutofocus"}},
        QJsonObject{{"rows.0.label",QString::fromUtf8("自动对焦 α")},{"rows.2.label",QString::fromUtf8("手动对焦 α")},{"rows.1.label","AF"}});
    translator.language=1;
    check(QJsonObject{{"op","set"},{"target","store"},{"name","languageIndex"},{"value",1}},
        QJsonObject{{"rows.0.label",QString::fromUtf8("自动对焦 β")},{"rows.2.label",QString::fromUtf8("手动对焦 β")},{"translations",1}});
    check(QJsonObject{{"op","set"},{"target","root"},{"name","itemValues"},{"value","main"}},
        QJsonObject{{"rows.1.label",QString::fromUtf8("开关 β")},{"rows.2.label","L1:Four"}});
    QCoreApplication::removeTranslator(&translator);
    check(QJsonObject{{"op","call"},{"method","rebuild"},{"args",QJsonArray()}},QJsonObject{{"rows.1.label","L1:Flag"},{"rows.0.label","L1:Heading"}});
    if(reason.isEmpty()&&(!baseline.warnings.isEmpty()||!candidate.warnings.isEmpty()))reason="qml-warning";
    report["records"]=records;report["assertions"]=assertions;report["passed"]=reason.isEmpty();
    if(!reason.isEmpty()){report["reason"]=reason;report["warnings"]=QJsonArray::fromStringList(baseline.warnings+candidate.warnings);}
    return report;
}
static QJsonObject microbenchmark(const QString &dir,const QJsonObject &inputs) {
    Fixture baseline(dir+"/baseline.qml",inputs,false),candidate(dir+"/candidate.qml",inputs,false);
    if(!baseline.root||!candidate.root)return QJsonObject{{"error","component"}};
    QJsonObject output{{"scope","无窗口纯UI规则微基准；不等于实际UI帧时延或设备总线耗时"},
        {"rows",14},{"batches",8},{"iterationsPerBatch",8},{"alternatingOrder",true}};
    for(int phase=0;phase<2;++phase) {
        QJSValue oldMethod=baseline.page.property("rebuild"),newMethod=candidate.page.property("rebuild");
        for(int warm=0;warm<8;++warm){oldMethod.callWithInstance(baseline.page);newMethod.callWithInstance(candidate.page);}
        qint64 oldTotal=0,newTotal=0;QJsonArray samples;
        auto measure=[&](Fixture &fixture,QJSValue &method,int start)->qint64 {
            QElapsedTimer timer;timer.start();
            for(int n=0;n<8;++n) {
                if(phase)fixture.store.insert("brightness",(start+n)%11);
                const auto result=method.callWithInstance(fixture.page);
                if(result.isError())fixture.warnings.append(result.toString());
            }
            return timer.nsecsElapsed();
        };
        for(int batch=0;batch<8;++batch) {
            qint64 oldTime,newTime;
            if(batch%2==0){oldTime=measure(baseline,oldMethod,batch*8);newTime=measure(candidate,newMethod,batch*8);}
            else {newTime=measure(candidate,newMethod,batch*8);oldTime=measure(baseline,oldMethod,batch*8);}
            oldTotal+=oldTime;newTotal+=newTime;
            samples.append(QJsonObject{{"baselineFirst",batch%2==0},{"baselineUsPerCall",double(oldTime)/8000},{"nativeUsPerCall",double(newTime)/8000}});
        }
        output[phase?"changedRows":"sameRows"]=QJsonObject{{"baselineUsPerCall",double(oldTotal)/64000},
            {"nativeUsPerCall",double(newTotal)/64000},{"nativeToBaselineRatio",oldTotal?double(newTotal)/oldTotal:0},{"samples",samples}};
    }
    if(!baseline.warnings.isEmpty()||!candidate.warnings.isEmpty())output["warnings"]=QJsonArray::fromStringList(baseline.warnings+candidate.warnings);
    return output;
}
int main(int argc,char **argv) {
    std::signal(SIGSEGV,crash);std::signal(SIGABRT,crash);
    alarm(60);QCoreApplication app(argc,argv);if(argc!=2)return 2;
    const QString dir=QString::fromLocal8Bit(argv[1]);
    singletonFixtureDirectory=dir;
    qmlRegisterSingletonType<QObject>("HblSettingsApiFixture",1,0,"Settings",settingsSingleton);
    const auto inputs=QJsonDocument::fromJson(bytes(dir+"/events.json")).object();const auto cases=inputs["cases"].toArray();
    if(cases.isEmpty())return 3;
    QElapsedTimer elapsed;elapsed.start();int records=0,assertions=0;
    QJsonObject report{{"passed",false},{"qtVersion",qVersion()},{"cameraRequests",0},{"cases",cases.size()},{"forcedGcInterval",17}};
    auto fail=[&](const QString &reason,const QString &caseName,int index,const QJsonObject &before,const QJsonObject &after,const QStringList &warnings)->int {
        report["reason"]=reason;report["case"]=caseName;report["index"]=index;report["records"]=records;report["elapsedMs"]=int(elapsed.elapsed());report["warnings"]=QJsonArray::fromStringList(warnings);
        save(dir+"/result.json",report);save(dir+"/failure.json",QJsonObject{{"reason",reason},{"case",caseName},{"index",index},{"baseline",before},{"candidate",after}});std::printf("failed\n");return 1;
    };
    int caseIndex=0;
    for(const auto &caseValue:cases) {
        checkpoint(caseIndex,-2,"create");
        const auto testCase=caseValue.toObject();const QString name=testCase["name"].toString();const bool withoutNative=testCase["withoutNative"].toBool();
        Fixture baseline(dir+"/baseline.qml",inputs,withoutNative),candidate(dir+"/candidate.qml",inputs,withoutNative);
        report["bridgeDiagnostics"]=candidate.bridgeDiagnostics();
        if(!baseline.root||!candidate.root)return fail("component",name,-1,QJsonObject(),QJsonObject(),baseline.warnings+candidate.warnings);
        auto events=testCase["events"].toArray();events.prepend(QJsonObject{{"op","reset"}});
        for(int i=0;i<events.size();++i) {
            const auto event=events[i].toObject();
            checkpoint(caseIndex,i-1,"baseline");HblSettingsRules::traceBudget(100000);
            const auto before=baseline.observe(event);
            checkpoint(caseIndex,i-1,"candidate");HblSettingsRules::traceBudget(100000);
            const auto after=candidate.observe(event);++records;
            HblSettingsRules::traceBudget(0);
            checkpoint(caseIndex,i-1,"compared");
            const auto warnings=baseline.warnings+candidate.warnings;
            if(!warnings.isEmpty())return fail("qml-warning",name,i-1,before,after,warnings);
            const auto diff=difference(before,after,"observed");if(!diff.isEmpty())return fail(diff,name,i-1,before,after,warnings);
            const auto expected=event["expect"].toObject();
            for(const auto &key:expected.keys()) {
                QJsonValue actual=before;for(const auto &part:key.split('.'))actual=actual.isArray()?actual.toArray().at(part.toInt()):actual.toObject().value(part);
                const auto mismatch=difference(expected[key],actual,key);if(!mismatch.isEmpty())return fail("baseline-assertion "+mismatch,name,i-1,before,after,warnings);++assertions;
            }
        }
        ++caseIndex;
    }
    checkpoint(caseIndex,-1,"translation");
    const auto translations=translationRegression(dir,inputs);report["translationRegression"]=translations;
    if(!translations["passed"].toBool())return fail("translation-regression","translation",translations["records"].toInt(),QJsonObject(),translations,QStringList());
    checkpoint(caseIndex+1,-1,"singleton");
    const auto singleton=singletonRegression(dir,inputs);report["singletonRegression"]=singleton;
    if(!singleton["passed"].toBool())return fail("singleton-regression","singleton",singleton["records"].toInt(),QJsonObject(),singleton,QStringList());
    checkpoint(caseIndex+2,-1,"benchmark");
    report["microbenchmark"]=microbenchmark(dir,inputs);
    report["passed"]=true;report["records"]=records;report["assertions"]=assertions;report["elapsedMs"]=int(elapsed.elapsed());
    save(dir+"/result.json",report);std::printf("passed %d records\n",records);return 0;
}
