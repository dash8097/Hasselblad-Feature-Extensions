#ifndef HBL_FOCUS_BRIDGE_H
#define HBL_FOCUS_BRIDGE_H
#include "focus_core.h"
#include <QtQml/QQmlPropertyMap>
#include <QtQml/QJSValue>
#include <QtCore/QTimer>
class __attribute__((visibility("hidden"))) NativeFocusCore:public QQmlPropertyMap {
    FocusCore state={};
    QTimer tick,deadline;
    int writes=0,completions=0,failures=0;
    void dispatch(int command,double value=0) {
        focus_dispatch(&state,command,value);
        const unsigned effects=state.effects;
        if(effects&FT_STOP)tick.stop();
        if(effects&FT_START)tick.start();
        if(effects&FD_STOP)deadline.stop();
        if(effects&FD_START)deadline.start();
        insert("v",QVariantList()<<focus_displayed(&state)<<bool(focus_pending(&state))<<bool(state.dragging)<<bool(state.active));
        if(effects&F_FAIL)insert("e2",++failures);
        if(effects&F_WRITE){insert("p",double(state.sent));insert("e0",++writes);}
        if(effects&F_DONE)insert("e1",++completions);
    }
public:
    explicit NativeFocusCore(QObject *parent):QQmlPropertyMap(parent) {
        tick.setInterval(16);tick.setSingleShot(true);
        deadline.setInterval(3000);deadline.setSingleShot(true);
        insert("request",QVariantList());insert("e0",0);insert("e1",0);insert("e2",0);insert("p",0.0);
        dispatch(-1);
        QObject::connect(&tick,&QTimer::timeout,[this](){dispatch(4);});
        QObject::connect(&deadline,&QTimer::timeout,[this](){dispatch(6);});
        QObject::connect(this,&QQmlPropertyMap::valueChanged,[this](const QString &name,const QVariant &value){
            if(name!="request")return;
            const QVariantList args=value.userType()==qMetaTypeId<QJSValue>()?value.value<QJSValue>().toVariant().toList():value.toList();
            insert("request",QVariantList());
            if(args.size()==2)dispatch(args[0].toInt(),args[1].toDouble());
        });
    }
};
#endif
