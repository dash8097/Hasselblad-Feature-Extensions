#ifndef HBL_SETTINGS_RULES_CORE_H
#define HBL_SETTINGS_RULES_CORE_H
#include <QtCore/QPointer>
#include <QtCore/QMetaProperty>
#include <QtCore/QHash>
#include <QtCore/QCoreApplication>
#include <QtQml/QQmlPropertyMap>
#include <QtQml/QQmlEngine>
#include <cmath>
#include <limits>
#ifdef HBL_SETTINGS_RULES_TRACE
#include <cstdio>
#endif

// X1D 1.25 UI adapter rules. The caller supplies existing QML objects and the
// clock. This class owns no transport, device, file, timer or camera interface.
// Original MenuItems condition expressions stay in their QML lexical scope.
namespace HblSettingsRules {
#ifdef HBL_SETTINGS_RULES_TRACE
static int traceRemaining=0;
static unsigned traceSequence=0;
static char traceRing[32][128]={{0}};
static int traceLengths[32]={0};
#endif
static void traceBudget(int count) {
#ifdef HBL_SETTINGS_RULES_TRACE
    traceRemaining=count;
    if(count>0)traceSequence=0;
#else
    Q_UNUSED(count);
#endif
}
static void trace(const char *stage,const char *name="",int value=0) {
#ifdef HBL_SETTINGS_RULES_TRACE
    if(traceRemaining>0){
        --traceRemaining;const unsigned slot=traceSequence%32;
        const int n=std::snprintf(traceRing[slot],128,"settings-checkpoint %s %s %d\n",stage,name,value);
        traceLengths[slot]=qMax(0,qMin(n,127));++traceSequence;
    }
#else
    Q_UNUSED(stage);Q_UNUSED(name);Q_UNUSED(value);
#endif
}
static QVariant unpack(const QVariant &v) {
    return v.userType()==qMetaTypeId<QJSValue>()?v.value<QJSValue>().toVariant():v;
}
static bool integer(const QVariant &v) {
    switch(v.type()) {
    case QVariant::Double: case QVariant::Int: case QVariant::UInt:
    case QVariant::LongLong: case QVariant::ULongLong:
        return std::isfinite(v.toDouble()) && std::floor(v.toDouble())==v.toDouble();
    default:return false;
    }
}
static double bounded(double value,double lo,double hi) {
    if(std::isnan(value)||std::isnan(lo)||std::isnan(hi))return std::numeric_limits<double>::quiet_NaN();
    return qMax(lo,qMin(hi,value));
}
static double quantize(double value,double lo,double hi,double step) {
    // Math.round ties toward +infinity, including negative half-steps.
    return bounded(lo+std::floor((value-lo)/step+0.5)*step,lo,hi);
}

class Page {
    QPointer<QObject> object;
    QPointer<QQmlEngine> engine;
    QJSValue objectHandle;
    QJSValue settingsHandle;
    QHash<QByteArray,QJSValue> bridgeMethods;
    QHash<QByteArray,QJSValue> settingsMethods;
    bool failed;
public:
    explicit Page(QObject *o):object(o),engine(o?qmlEngine(o):nullptr),failed(false) {if(object&&engine)objectHandle=wrap(object);}
    QJSValue wrap(QObject *instance)const {
        if(!instance||!engine)return QJSValue(QJSValue::NullValue);
        // Qt5.5 newQObject otherwise changes implicit C++ ownership to JS
        // ownership. These are borrowed existing UI/proxy objects.
        const QQmlEngine::ObjectOwnership ownership=QQmlEngine::objectOwnership(instance);
        const QJSValue value=engine->newQObject(instance);
        if(QQmlEngine::objectOwnership(instance)!=ownership)QQmlEngine::setObjectOwnership(instance,ownership);
        return value;
    }
    QJSValue script(const QVariant &v)const {
        if(v.userType()==qMetaTypeId<QJSValue>())return v.value<QJSValue>();
        switch(v.type()) {
        case QVariant::Bool:return QJSValue(v.toBool());
        case QVariant::String:return QJSValue(v.toString());
        case QVariant::Int:case QVariant::UInt:case QVariant::LongLong:
        case QVariant::ULongLong:case QVariant::Double:return QJSValue(v.toDouble());
        default:break;
        }
        // Qt5.5 does not wrap a QVariant holding a QQuickItem* alias as a
        // QObject when passed through generic toScriptValue(QVariant).
        trace("script-variant",v.typeName()?v.typeName():"invalid",v.userType());
        QObject *instance=v.value<QObject*>();
        trace("script-object", "",instance?1:0);
        if(instance&&engine){trace("script-wrap");const QJSValue value=wrap(instance);trace("script-wrapped");return value;}
        const QJSValue value=engine?engine->toScriptValue(v):QJSValue();trace("script-converted");return value;
    }
    QJSValue get(const char *name)const {return object?script(object->property(name)):QJSValue();}
    QJSValue self()const {return object&&engine?objectHandle:QJSValue();}
    void set(const char *name,const QJSValue &value) {
        if(!object)return;
        const int index=object->metaObject()->indexOfProperty(name);
        const int type=index>=0?object->metaObject()->property(index).userType():QMetaType::QVariant;
        trace("set",name,type);
        const QVariant input=type==QMetaType::QVariant||type==qMetaTypeId<QJSValue>()?QVariant::fromValue(value):value.toVariant();
        if(!object->setProperty(name,input)&&index>=0)failed=true;
        trace("set-done",name,failed?0:1);
    }
    QJSValue invoke(QJSValue method,const QJSValue &target,const char *name,const QJSValueList &args) {
        trace("call",name);
        if(!method.isCallable()){failed=true;return QJSValue();}
        QJSValue result=method.callWithInstance(target,args);
        trace("call-done",name,result.isError()?0:1);
        if(result.isError())failed=true;
        return result;
    }
    QJSValue call(const QJSValue &target,const char *name,const QJSValueList &args=QJSValueList()) {
        return invoke(target.property(QString::fromLatin1(name)),target,name,args);
    }
    QJSValue bridge(const char *name,const QJSValueList &args=QJSValueList()) {
        // Fixed QML methods only, cached for this operation. No live values or
        // condition results survive the operation or are reused between rows.
        const QByteArray key(name);auto found=bridgeMethods.constFind(key);
        if(found==bridgeMethods.constEnd()){bridgeMethods.insert(key,self().property(QString::fromLatin1(name)));found=bridgeMethods.constFind(key);}
        return invoke(found.value(),self(),name,args);
    }
    QJSValue settings(const char *name,const QJSValueList &args) {
        // Keep Qt5.5's original singleton type wrapper and bound methods.
        // This is not necessarily a QJSValue::isQObject() value. The handle
        // and fixed methods only live for this operation; results are live.
        if(settingsHandle.isUndefined())settingsHandle=get("nativeSettingsApi");
        const QByteArray key(name);auto found=settingsMethods.constFind(key);
        if(found==settingsMethods.constEnd()){settingsMethods.insert(key,settingsHandle.property(QString::fromLatin1(name)));found=settingsMethods.constFind(key);}
        return invoke(found.value(),settingsHandle,name,args);
    }
    bool condition(const QJSValue &value) {
        if(value.isUndefined()||value.isNull()||(value.isString()&&value.toString().isEmpty()))return true;
        return bridge("condition",QJSValueList()<<value).toBool();
    }
    QJSValue translate(const QJSValue &text) {
        // Qt5.5.1 GlobalExtensions::method_qsTranslate with exactly the
        // original two string arguments. An empty disambiguation (not null)
        // and n=-1 match that implementation. Keep its original type-error
        // path for malformed non-string menu metadata.
        if(!text.isString())return bridge("nativeTranslate",QJSValueList()<<text);
        const QByteArray source=text.toString().toUtf8();
        return QJSValue(QCoreApplication::translate("MENUS",source.constData(),"",-1));
    }
    bool type(const QJSValue &entry,int value)const {return entry.property("editType").strictlyEquals(QJSValue(value));}
    QJSValue array()const {return engine->newArray();}
    void append(QJSValue &list,const QJSValue &value) {list.setProperty(list.property("length").toUInt(),value);}
    QJSValue entry(const QVariant &index)const {
        QJSValue list=get("entries");
        return integer(index)&&index.toDouble()>=0&&index.toDouble()<list.property("length").toUInt()?list.property(index.toUInt()):QJSValue();
    }
    QJSValue modeLabel(const QJSValue &index,const char *const *labels,int count)const {
        const double value=index.toNumber();
        if(!std::isfinite(value)||std::floor(value)!=value||value<0||value>=count)return QJSValue();
        return QJSValue(QString::fromUtf8(labels[int(value)]));
    }
    void rebuild(double now) {
        trace("rebuild-begin");
        QJSValue page=get("nativePage");
        if(page.property("adjusting").toBool())return;
        QJSValue data=get("specification"),out=array(),bindings=array();
        QJSValue pendingValues=get("pendingSliderValues");
        const unsigned count=data.property("length").toUInt();
        for(unsigned i=0;i<count&&!failed;++i) {
            const QJSValue e=data.property(i),proxy=e.property("proxy");
            if((e.property("demo").toBool()&&get("nativeHideDemoItems").toBool())||!condition(e.property("validCheck")))continue;
            const bool enabled=condition(e.property("enableCond"));
            const QJSValue editType=e.property("editType"),nameValue=e.property("name");
            const double editor=editType.isNumber()?editType.toNumber():std::numeric_limits<double>::quiet_NaN();
            if(editor==7&&!enabled)continue;
            const QString name=nameValue.toString();
            const bool heading=editor==9,special=name=="WIFI_power"||name=="CustomOption_LiveViewEVFOnly";
            const bool toggle=!special&&(editor==2||editor==4),hasProxy=proxy.toBool();
            QJSValue value=!heading&&editor!=3&&hasProxy?proxy.property(name):QJSValue(false);
            if(editor==5) {
                const QJSValue pending=pendingValues.property(name);
                if(pending.toBool()) {
                    const QJSValue chosen=pending.property("value");
                    if(std::abs(value.toNumber()-chosen.toNumber())<0.00001||now>pending.property("deadline").toNumber())pendingValues.deleteProperty(name);
                    else value=chosen;
                }
            }
            QJSValue display=!heading&&!toggle&&editor!=3&&hasProxy?settings("getDisplayValue",QJSValueList()<<nameValue<<value):QJSValue(QString());
            if(editor==8)display=value;
            const QJSValue text2=e.property("text2"),suppress=e.property("suppressUnitOn");
            QJSValue unit=text2.toBool()?text2:QJSValue(QString());
            if(suppress.toBool()&&hasProxy&&settings("getUntranslatedDisplayValue",QJSValueList()<<nameValue<<value).strictlyEquals(suppress))unit=QJSValue(QString());
            const QString kind=heading?"heading":toggle?"toggle":editor==7?"text":editor==5?"slider":editor==3?"action":"choice";
            // Qt5.5.1 compiles the original object literal's initial keys in
            // this order; retain its exact JSON fingerprint, then append the
            // slider fields in their original assignment order.
            const QVariantMap initial{{"description",toggle&&text2.toBool()?text2.toVariant():QVariant(QString())},
                {"enabled",enabled},{"kind",kind},{"label",translate(e.property("text1")).toVariant()},
                {"value",value.toBool()},{"valueText",display.toString()+(unit.toBool()?QStringLiteral(" ")+unit.toString():QString())}};
            QJSValue row=engine->toScriptValue(initial);
            if(name=="image_format"&&get("formatLocked").toBool()) {row.setProperty("kind","text");row.setProperty("enabled",false);}
            if(special) {
                const QJSValue radio=get("nativeRadio");
                const bool present=!radio.isUndefined();
                row.setProperty("kind","choice");
                if(name=="WIFI_power") {
                    static const char *const labels[]={"关","Wi-Fi","引闪"};
                    row.setProperty("valueText",present?modeLabel(radio.property("radioMode"),labels,3):QJSValue(QString::fromUtf8("未就绪")));
                    row.setProperty("enabled",enabled&&present&&radio.property("radioModeVerified").toBool()&&!radio.property("radioModeBusy").toBool());
                    row.setProperty("description",present?(radio.property("radioModeBusy").toBool()?QJSValue(QString::fromUtf8("切换中")):radio.property("radioModeError")):QJSValue(QString()));
                }else {
                    static const char *const labels[]={"正常","电子取景器","屏幕取景"};
                    row.setProperty("valueText",modeLabel(present&&radio.property("viewfinderMode").toNumber()>=0?radio.property("viewfinderMode"):QJSValue(value.toBool()?1:0),labels,3));
                }
            }
            if(kind=="slider") {
                row.setProperty("numberValue",value);
                row.setProperty("minimum",settings("getMinValue",QJSValueList()<<nameValue));
                row.setProperty("maximum",settings("getMaxValue",QJSValueList()<<nameValue));
                row.setProperty("step",settings("getStepsForValue",QJSValueList()<<nameValue));
            }
            append(out,row);append(bindings,e);
        }
        if(failed)return;
        trace("rebuild-rows-ready");
        set("entries",bindings);
        // Fixed standard serialization only; no source text/eval or business closures.
        const QJSValue json=engine->globalObject().property("JSON");
        const QJSValue fingerprint=call(json,"stringify",QJSValueList()<<out);
        if(!failed&&!fingerprint.strictlyEquals(get("lastRows"))) {
            set("lastRows",fingerprint);
            trace("rebuild-assign-rows");
            page.setProperty("rows",out);
            trace("rebuild-assigned-rows");
        }
    }
    void refresh(double now) {
        const QJSValue key=get("itemValues");
        QJSValue spec=bridge("nativeSpecification",QJSValueList()<<key);
        if(key.strictlyEquals(QJSValue(QStringLiteral("cameraSettingsAutofocus")))) {
            QJSValue joined=array(),heading=engine->newObject();
            heading.setProperty("editType",9);heading.setProperty("text1",translate(QJSValue(QStringLiteral("Auto Focus"))));append(joined,heading);
            for(unsigned i=0;i<spec.property("length").toUInt();++i)append(joined,spec.property(i));
            heading=engine->newObject();heading.setProperty("editType",9);heading.setProperty("text1",translate(QJSValue(QStringLiteral("Manual Focus"))));append(joined,heading);
            spec=bridge("nativeSpecification",QJSValueList()<<QJSValue(QStringLiteral("cameraSettingsManualFocus")));
            for(unsigned i=0;i<spec.property("length").toUInt();++i)append(joined,spec.property(i));
            spec=joined;
        }
        if(!failed){set("specification",spec);rebuild(now);}
    }
    bool permitted(const QJSValue &e) {return e.toBool()&&condition(e.property("enableCond"))&&condition(e.property("validCheck"))&&!failed;}
    void toggle(const QVariant &index,const QJSValue &value,double now) {
        const QJSValue e=entry(index);QJSValue proxy=e.property("proxy");
        if(!e.toBool()||!proxy.toBool()||!permitted(e))return;
        const QString name=e.property("name").toString();
        if(name=="ram_only_mode")call(get("nativeActions"),"run",QJSValueList()<<e);
        else proxy.setProperty(name,value);
        if(!failed)rebuild(now);
    }
    void edit(const QVariant &index) {
        const QJSValue e=entry(index);
        const QString name=e.property("name").toString();
        if(!e.toBool()||(name=="image_format"&&get("formatLocked").toBool())||!permitted(e))return;
        const QJSValue page=get("nativePage"),radio=get("nativeRadio"),proxy=e.property("proxy");
        if(type(e,3)||type(e,8))call(get("nativeActions"),"run",QJSValueList()<<e);
        else if(name=="WIFI_power") {
            if(!radio.isUndefined()&&radio.property("radioModeVerified").toBool()&&!radio.property("radioModeBusy").toBool())call(get("nativeRadioChooser"),"openRadio",QJSValueList()<<page<<radio.property("radioMode"));
        }else if(name=="CustomOption_LiveViewEVFOnly") {
            call(get("nativeChooser"),"openViewfinder",QJSValueList()<<page<<(!radio.isUndefined()&&radio.property("viewfinderMode").toNumber()>=0?radio.property("viewfinderMode"):QJSValue(proxy.property(name).toBool()?1:0)));
        }else if(proxy.toBool())call(get("nativeChooser"),"open",QJSValueList()<<page<<proxy<<e.property("name"));
    }
    void chooseValue(const QVariant &index,const QJSValue &value,double now) {
        const QJSValue e=entry(index);QJSValue proxy=e.property("proxy");
        if(!e.toBool()||!proxy.toBool()||!type(e,5)||!permitted(e))return;
        const QJSValue name=e.property("name");
        const double lo=settings("getMinValue",QJSValueList()<<name).toNumber(),hi=settings("getMaxValue",QJSValueList()<<name).toNumber();
        const QJSValue step=settings("getStepsForValue",QJSValueList()<<name);
        const double chosen=quantize(value.toNumber(),lo,hi,step.toBool()?step.toNumber():1);
        if(failed)return;
        QJSValue pending=engine->newObject(),pendingValues=get("pendingSliderValues");
        pending.setProperty("value",chosen);pending.setProperty("deadline",now+2000);
        pendingValues.setProperty(name.toString(),pending);
        proxy.setProperty(name.toString(),chosen);rebuild(now);
    }
    void syncRows() {
        trace("sync-begin");
        QJSValue rows=get("rows"),model=get("nativeRowModel");
        const QJSValue setter=model.property("set"),appender=model.property("append");
        const QVariantMap defaults{{"kind","text"},{"label",""},{"enabled",true},{"value",false},
            {"valueText",""},{"description",""},{"numberValue",0},{"minimum",0},{"maximum",1},{"step",1}};
        const unsigned count=rows.property("length").toUInt();
        trace("sync-count","",int(count));
        for(unsigned i=0;i<count&&!failed;++i) {
            trace("sync-row","",int(i));
            // Snapshot the data row before allocating/calling into QML again.
            // Qt5.5 QJSValueIterator keeps unrooted current/next names and
            // properties; never keep such an iterator alive across allocation
            // or a reentrant ListModel notification.
            const QJSValue sourceRow=rows.property(i);
            const QVariantMap fields=sourceRow.toVariant().toMap();
            trace("sync-snapshot");
            QVariantMap merged=defaults;
            for(auto field=fields.constBegin();field!=fields.constEnd();++field)merged.insert(field.key(),field.value());
            const QJSValue item=engine->toScriptValue(merged);
            trace("sync-item");
            if(i<model.property("count").toUInt())invoke(setter,model,"set",QJSValueList()<<QJSValue(i)<<item);
            else invoke(appender,model,"append",QJSValueList()<<item);
        }
        if(!failed&&model.property("count").toUInt()>count)call(model,"remove",QJSValueList()<<QJSValue(count)<<QJSValue(model.property("count").toUInt()-count));
    }
    void sliderValue(QJSValue touch,const QJSValue &row,const QJSValue &index,double px,double now) {
        const double lo=row.property("minimum").toNumber(),hi=row.property("maximum").toNumber();
        const QJSValue step=row.property("step");
        const double value=lo+bounded((px-12)/(touch.property("width").toNumber()-24),0,1)*(hi-lo);
        const double preview=quantize(value,lo,hi,step.toBool()?step.toNumber():1);
        touch.setProperty("previewValue",preview);touch.setProperty("awaitingValue",true);touch.setProperty("readbackDeadline",now+2000);
        call(self(),"valueRequested",QJSValueList()<<index<<QJSValue(preview));
    }
    void sliderReadback(QJSValue touch,const QJSValue &row,double now) {
        if(std::abs(row.property("numberValue").toNumber()-touch.property("previewValue").toNumber())<0.00001||now>touch.property("readbackDeadline").toNumber())touch.setProperty("awaitingValue",false);
    }
    QVariant run(int op,const QVariantList &args) {
        trace("run","",op);
        if(!object||!engine)return false;
        auto arg=[&args](int i)->QVariant{return i<args.size()?args[i]:QVariant();};
        switch(op) {
        case 1:rebuild(unpack(arg(0)).toDouble());break;
        case 2:refresh(unpack(arg(0)).toDouble());break;
        case 3:toggle(unpack(arg(0)),script(arg(1)),unpack(arg(2)).toDouble());break;
        case 4:edit(unpack(arg(0)));break;
        case 5:chooseValue(unpack(arg(0)),script(arg(1)),unpack(arg(2)).toDouble());break;
        case 6:bridge("nativeRefreshTranslations");if(!failed)refresh(unpack(arg(0)).toDouble());break;
        case 20:syncRows();break;
        case 21:sliderValue(script(arg(0)),script(arg(1)),script(arg(2)),unpack(arg(3)).toDouble(),unpack(arg(4)).toDouble());break;
        case 22:sliderReadback(script(arg(0)),script(arg(1)),unpack(arg(2)).toDouble());break;
        default:return false;
        }
        trace("run-done","",op);return !failed;
    }
};
}

class __attribute__((visibility("hidden"))) NativeSettingsRules:public QQmlPropertyMap {
public:
    explicit NativeSettingsRules(QObject *parent):QQmlPropertyMap(parent) {
        insert("request",QVariantList());insert("result",false);
        QObject::connect(this,&QQmlPropertyMap::valueChanged,this,[this](const QString &name,const QVariant &v) {
            if(name!="request")return;
            const QVariantList input=HblSettingsRules::unpack(v).toList();insert("request",QVariantList());
            QVariant result=false;
            if(input.size()==3&&HblSettingsRules::integer(HblSettingsRules::unpack(input[1]))) {
                HblSettingsRules::Page page(HblSettingsRules::unpack(input[0]).value<QObject*>());
                result=page.run(HblSettingsRules::unpack(input[1]).toInt(),HblSettingsRules::unpack(input[2]).toList());
            }
            const bool blocked=blockSignals(true);insert("result",result);blockSignals(blocked);
        });
    }
};
#endif
