#ifndef HBL_RF_INT_RECEIVER_H
#define HBL_RF_INT_RECEIVER_H
#include <QtCore/qobject.h>
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#include <functional>
#include <cstring>

// Qt 5.5.1 的单个 int slot 布局；调用方仅计事件，不保存曝光参数。
class RfIntReceiver : public QObject {
public:
    explicit RfIntReceiver(QObject *parent=nullptr) : QObject(parent) {}
    std::function<void()> received;
    static const QMetaObject staticMetaObject;
    const QMetaObject *metaObject() const override { return &staticMetaObject; }
    void *qt_metacast(const char *name) override {
        if (!name) return nullptr;
        if (!std::strcmp(name,"RfIntReceiver")) return static_cast<void *>(this);
        return QObject::qt_metacast(name);
    }
    int qt_metacall(QMetaObject::Call call,int id,void **args) override {
        id=QObject::qt_metacall(call,id,args);
        if (id<0) return id;
        if (call==QMetaObject::InvokeMetaMethod) {
            if (id==0) invoke(this,call,0,args);
            --id;
        } else if (call==QMetaObject::RegisterMethodArgumentMetaType) {
            if (id==0) *reinterpret_cast<int *>(args[0])=-1;
            --id;
        }
        return id;
    }
private:
    static void invoke(QObject *object,QMetaObject::Call call,int id,void **) {
        auto receiver=static_cast<RfIntReceiver *>(object);
        if (call==QMetaObject::InvokeMetaMethod && id==0 && receiver->received) receiver->received();
    }
};
struct RfIntReceiverStrings { QByteArrayData data[4]; char text[29]; };
#define RF_INT_STRING(index,offset,length) Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(length,offsetof(RfIntReceiverStrings,text)+offset-index*sizeof(QByteArrayData))
static const RfIntReceiverStrings rfIntReceiverStrings={
    {RF_INT_STRING(0,0,13),RF_INT_STRING(1,14,7),RF_INT_STRING(2,22,0),RF_INT_STRING(3,23,5)},
    "RfIntReceiver\0receive\0\0value"
};
#undef RF_INT_STRING
static const uint rfIntReceiverMetadata[]={
    7,0,0,0,1,14,0,0,0,0,0,0,0,0,
    1,1,19,2,0x0a,
    QMetaType::Void,QMetaType::Int,3,
    0
};
const QMetaObject RfIntReceiver::staticMetaObject={
    {&QObject::staticMetaObject,rfIntReceiverStrings.data,rfIntReceiverMetadata,&RfIntReceiver::invoke,nullptr,nullptr}
};
#endif
