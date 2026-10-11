#ifndef HBL_PAGE_POOL_CORE_H
#define HBL_PAGE_POOL_CORE_H

#include <QtCore/QMap>
#include <QtCore/QPointer>
#include <QtCore/QSet>
#include <QtCore/QSharedPointer>
#include <QtCore/QTimer>
#include <QtCore/QVariant>
#include <QtQml/QQmlEngine>
#include <QtQml/QQmlPropertyMap>
#include <QtQml/QJSValue>

// Qt 5.5.1 public API only. No moc, private Qt types, script source or eval.
// Each host has independent native cache and presentation state. QML owns the
// visual Loaders; retirement removes them from this cache before deleteLater().
namespace HblPagePool {
static QObject *object(const QVariant &v) {
    return v.userType()==qMetaTypeId<QJSValue>() ? v.value<QJSValue>().toQObject() : v.value<QObject*>();
}
static QString string(const QVariant &v) {
    return v.userType()==qMetaTypeId<QJSValue>() ? v.value<QJSValue>().toString() : v.toString();
}
static bool boolean(const QVariant &v) {
    return v.userType()==qMetaTypeId<QJSValue>() ? v.value<QJSValue>().toBool() : v.toBool();
}
static QVariantList array(const QVariant &v) {
    if(v.userType()!=qMetaTypeId<QJSValue>())return v.toList();
    const QJSValue a=v.value<QJSValue>();QVariantList out;
    if(!a.isArray())return out;
    const int count=a.property("length").toInt();
    for(int i=0;i<count;++i)out.append(QVariant::fromValue(a.property(i)));
    return out;
}
static QJSValue script(QQmlEngine *engine,const QVariant &v) {
    return v.userType()==qMetaTypeId<QJSValue>() ? v.value<QJSValue>() : engine->toScriptValue(v);
}
static QVariant arg(const QVariantList &a,int i) {return i<a.size()?a[i]:QVariant();}

class Pool : public QObject, public QEnableSharedFromThis<Pool> {
    enum Status {Null=0,Ready=1,Loading=2,Error=3};
    enum Preparation {Pending,Preparing,Prepared,Failed};
    struct Entry {
        QString key;
        QPointer<QObject> loader;
        QPointer<QObject> preparedItem;
        bool retained=false;
        Preparation preparation=Pending;
        QString failure;
    };
    typedef QSharedPointer<Entry> Slot;
    QPointer<QObject> owner;
    QMap<QString,Slot> cache;
    Slot selected;
    QTimer dispatch;
    bool active=false,delivered=false,failed=false,disposed=false;
    quint64 revision=0;
    QString selectionFailure;

    bool alive()const {return owner&&!disposed;}
    bool contains(const Slot &e)const {return e&&cache.value(e->key)==e&&e->loader;}
    int status(const Slot &e)const {return e&&e->loader?e->loader->property("status").toInt():Null;}
    QPointer<QObject> item(const Slot &e)const {return e&&e->loader?object(e->loader->property("item")):nullptr;}
    Slot lookup(QObject *loader)const {
        if(loader)for(auto i=cache.constBegin();i!=cache.constEnd();++i)if(i.value()->loader==loader)return i.value();
        return Slot();
    }
    void put(const char *name,const QVariant &v) {if(alive())owner->setProperty(name,v);}
    void schedule() {if(alive()&&active)dispatch.start();}
    quint64 resetPresentation() {
        const quint64 next=++revision;
        dispatch.stop();delivered=false;failed=false;
        put("delivered",false);
        return next;
    }
    void publishSlots() {
        if(!alive())return;
        QQmlEngine *engine=qmlEngine(owner);if(!engine)return;
        QJSValue values=engine->newObject();
        for(auto i=cache.constBegin();i!=cache.constEnd();++i)
            if(i.value()->loader)values.setProperty(i.key(),engine->newQObject(i.value()->loader));
        put("slots",QVariant::fromValue(values));
    }
    QJSValue invoke(QObject *target,const char *method,const QVariantList &arguments,QString *error,bool optional=false) {
        if(!alive()||!target)return QJSValue();
        QQmlEngine *engine=qmlEngine(target);
        if(!engine){if(error)*error=QStringLiteral("Page has no QML engine");return QJSValue();}
        QJSValue self=engine->newQObject(target),function=self.property(QString::fromLatin1(method));
        if(!function.isCallable()) {
            if(!optional&&error)*error=QStringLiteral("Missing page bridge: ")+QString::fromLatin1(method);
            return QJSValue();
        }
        QJSValueList values;for(const auto &v:arguments)values.append(script(engine,v));
        QJSValue result=function.callWithInstance(self,values);
        if(result.isError()&&error)*error=result.toString();
        return result;
    }
    void destroyed(const Slot &entry) {
        if(!alive()||cache.value(entry->key)!=entry)return;
        cache.remove(entry->key);
        if(selected==entry) {
            selected.reset();selectionFailure=QStringLiteral("Selected page was destroyed");
            const quint64 next=resetPresentation();
            if(!alive()||revision!=next)return;
            put("selected",QVariant::fromValue(static_cast<QObject*>(nullptr)));
        }
        publishSlots();schedule();
    }
    void retire(const Slot &entry) {
        if(!contains(entry))return;
        QPointer<QObject> loader=entry->loader;
        cache.remove(entry->key);
        // Any synchronous callback now sees the entry as retired. It cannot
        // prepare, dispatch, or erase a newly acquired entry with the same key.
        publishSlots();
        if(loader)loader->deleteLater();
    }
    void choose(const Slot &entry,const QString &failure=QString()) {
        if(!alive())return;
        if(selected==entry&&selectionFailure==failure)return;
        const Slot previous=selected;
        selected=entry;selectionFailure=failure;
        const quint64 next=resetPresentation();
        if(!alive()||revision!=next)return;
        put("selected",QVariant::fromValue(entry?entry->loader.data():static_cast<QObject*>(nullptr)));
        // Property notifications and resident callbacks are synchronous. Never
        // overwrite a newer selection or presentation after they return.
        if(alive()&&previous&&previous!=selected&&!previous->retained)retire(previous);
        if(alive()&&revision==next)schedule();
    }
    Slot acquire(const QVariantList &a,QString *failure) {
        if(!alive())return Slot();
        const QString key=string(arg(a,0));
        const Slot existing=cache.value(key);
        if(existing&&existing->loader)return existing;
        QString error;
        const QJSValue made=invoke(owner,"_createSlot",QVariantList()<<key<<boolean(arg(a,3)),&error);
        QPointer<QObject> loader=made.toQObject();
        if(!alive())return Slot();
        if(!loader||!error.isEmpty()) {
            if(failure)*failure=error.isEmpty()?QStringLiteral("Page loader could not be created"):error;
            return Slot();
        }
        Slot entry(new Entry);entry->key=key;entry->loader=loader;entry->retained=boolean(arg(a,3));
        cache.insert(key,entry);
        const QWeakPointer<Pool> weak=sharedFromThis();const QWeakPointer<Entry> weakEntry=entry;
        QObject::connect(loader,&QObject::destroyed,this,[weak,weakEntry](){
            const auto pool=weak.toStrongRef();const auto slot=weakEntry.toStrongRef();
            if(pool&&slot)pool->destroyed(slot);
        });
        publishSlots();
        if(!alive()||!contains(entry))return Slot();
        // Registration precedes setSource: a synchronous onLoaded can already
        // resolve this entry. Keep the original JS properties object intact so
        // Loader applies initial values before Component.onCompleted.
        invoke(loader,"_setSource",QVariantList()<<arg(a,1)<<arg(a,2),&error);
        if(contains(entry)&&!error.isEmpty()) {
            entry->preparation=Failed;entry->failure=error;
        }
        return contains(entry)?entry:Slot();
    }
    void loaded(QObject *loader) {
        const Slot entry=lookup(loader);
        if(!alive()||!contains(entry)||status(entry)!=Ready)return;
        const QPointer<QObject> page=item(entry);
        if(!page)return;
        if(entry->preparedItem==page&&entry->preparation!=Pending)return;
        entry->preparedItem=page;entry->preparation=Preparing;entry->failure.clear();
        QString error;
        invoke(page,"residentPrepare",QVariantList(),&error,true);
        // Even if preparation changes selection, deactivate this same page
        // before considering delivery. A retired Loader is deleted later.
        if(alive()&&page&&error.isEmpty())invoke(page,"residentDeactivate",QVariantList(),&error,true);
        if(!alive()||!contains(entry)||!page)return;
        entry->preparation=error.isEmpty()?Prepared:Failed;entry->failure=error;
        if(selected==entry)schedule();
    }
    void statusChanged(QObject *loader) {
        const Slot entry=lookup(loader);
        if(!alive()||!entry)return;
        if(status(entry)==Loading||status(entry)==Null) {
            entry->preparation=Pending;entry->preparedItem.clear();entry->failure.clear();
        }
        if(selected==entry)schedule();
    }
    void activeChanged() {
        if(!alive())return;
        const bool now=owner->property("active").toBool();
        if(now==active)return;
        active=now;
        const quint64 next=resetPresentation();
        if(!alive()||revision!=next)return;
        if(active)schedule();
        else if(selected&&!selected->retained)choose(Slot());
    }
    void selectedChanged(QObject *loader) {
        if(!alive()||(selected&&selected->loader==loader)||(!selected&&!loader))return;
        choose(lookup(loader));
    }
public:
    explicit Pool(QObject *host):owner(host),active(host->property("active").toBool()) {
        dispatch.setInterval(0);dispatch.setSingleShot(true);
    }
    void start() {
        const QWeakPointer<Pool> weak=sharedFromThis();
        QObject::connect(&dispatch,&QTimer::timeout,this,[weak](){const auto pool=weak.toStrongRef();if(pool)pool->deliver();});
    }
    void dispose() {disposed=true;dispatch.stop();cache.clear();selected.reset();owner.clear();}
    bool isLoading()const {
        for(auto i=cache.constBegin();i!=cache.constEnd();++i)if(status(i.value())==Loading)return true;
        return false;
    }
    void deliver() {
        if(!alive()||!active||delivered||failed)return;
        const Slot entry=selected;QString error=selectionFailure;
        if(entry&&entry->preparation==Failed)error=entry->failure;
        else if(entry&&status(entry)==Error)error=QStringLiteral("Page loading failed");
        if(!error.isEmpty()) {
            failed=true;
            QMetaObject::invokeMethod(owner,"loadFailed",Qt::DirectConnection,Q_ARG(QString,error));
            return;
        }
        if(!contains(entry)||status(entry)!=Ready||entry->preparation!=Prepared)return;
        const QPointer<QObject> page=item(entry);if(!page)return;
        const quint64 current=revision;
        delivered=true;put("delivered",true);
        if(!alive()||revision!=current||!active||selected!=entry||!page)return;
        // Do not read the public item binding here: on Qt5 it can still be
        // pending when active changes. Pass the Loader's actual object.
        const QVariant payload=QVariant::fromValue(page.data());
        QMetaObject::invokeMethod(owner,"loaded",Qt::DirectConnection,Q_ARG(QVariant,payload));
    }
    QVariant run(int operation,const QVariantList &a) {
        switch(operation) {
        case 0:return isLoading();
        case 1:{QString error;const Slot e=acquire(a,&error);return QVariant::fromValue(e?e->loader.data():static_cast<QObject*>(nullptr));}
        case 2:{QVariantList warm=a;while(warm.size()<4)warm.append(QVariant());warm[3]=true;QString error;acquire(warm,&error);break;}
        case 3:{QString error;const Slot e=acquire(a,&error);choose(e,error);break;}
        case 4:deliver();break;
        case 5:activeChanged();break;
        case 6:selectedChanged(object(arg(a,0)));break;
        case 7:loaded(object(arg(a,0)));break;
        case 8:statusChanged(object(arg(a,0)));break;
        default:break;
        }
        return QVariant();
    }
};
}

class __attribute__((visibility("hidden"))) NativePagePoolCore:public QQmlPropertyMap {
    QMap<QObject*,QSharedPointer<HblPagePool::Pool>> pools;
    QSet<QObject*> closing;
    void remove(QObject *owner) {
        const auto pool=pools.take(owner);if(pool)pool->dispose();
    }
public:
    explicit NativePagePoolCore(QObject *parent):QQmlPropertyMap(parent) {
        insert("request",QVariantList());insert("result",QVariant());
        QObject::connect(this,&QQmlPropertyMap::valueChanged,this,[this](const QString &name,const QVariant &value){
            if(name!=QStringLiteral("request"))return;
            const QVariantList input=HblPagePool::array(value);
            insert("request",QVariantList());QVariant result;
            if(input.size()==3) {
                QObject *owner=HblPagePool::object(input[0]);
                const int operation=HblPagePool::string(input[1]).toInt();
                if(owner&&operation==9) {
                    if(!closing.contains(owner)) {
                        closing.insert(owner);remove(owner);
                        QObject::connect(owner,&QObject::destroyed,this,[this,owner](){closing.remove(owner);remove(owner);});
                    }
                }
                else if(owner&&!closing.contains(owner)&&operation>=0&&operation<=8) {
                    auto pool=pools.value(owner);
                    if(!pool) {
                        pool.reset(new HblPagePool::Pool(owner));pools.insert(owner,pool);pool->start();
                        QObject::connect(owner,&QObject::destroyed,this,[this,owner](){remove(owner);});
                    }
                    result=pool->run(operation,HblPagePool::array(input[2]));
                }
            }
            const bool blocked=blockSignals(true);insert("result",result);blockSignals(blocked);
        });
    }
    ~NativePagePoolCore() {for(const auto &pool:pools)pool->dispose();}
    int poolCount()const {return pools.size();}
};
#endif
