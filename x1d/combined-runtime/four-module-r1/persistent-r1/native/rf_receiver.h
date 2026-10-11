#ifndef HBL_RF_RECEIVER_H
#define HBL_RF_RECEIVER_H
#include <QtCore/qobject.h>
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#include <functional>
#include <cstring>

// 与 Qt 5.5 moc 的单个无参数 slot 布局相同。DBus 的直接 slot 调用不依赖 sender()。
class RfReceiver : public QObject {
public:
    explicit RfReceiver(QObject *parent=nullptr) : QObject(parent) {}
    std::function<void()> received;
    static const QMetaObject staticMetaObject;
    const QMetaObject *metaObject() const override { return &staticMetaObject; }
    void *qt_metacast(const char *name) override {
        if (!name) return nullptr;
        if (!std::strcmp(name,"RfReceiver")) return static_cast<void *>(this);
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
        auto receiver=static_cast<RfReceiver *>(object);
        if (call==QMetaObject::InvokeMetaMethod && id==0 && receiver->received) receiver->received();
    }
};
struct RfReceiverStrings { QByteArrayData data[3]; char text[20]; };
#define RF_STRING(index,offset,length) Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(length,offsetof(RfReceiverStrings,text)+offset-index*sizeof(QByteArrayData))
static const RfReceiverStrings rfReceiverStrings={
    {RF_STRING(0,0,10),RF_STRING(1,11,7),RF_STRING(2,19,0)},
    "RfReceiver\0receive\0"
};
#undef RF_STRING
static const uint rfReceiverMetadata[]={
    7,0,0,0,1,14,0,0,0,0,0,0,0,0,
    1,0,19,2,0x0a,
    QMetaType::Void,
    0
};
const QMetaObject RfReceiver::staticMetaObject={
    {&QObject::staticMetaObject,rfReceiverStrings.data,rfReceiverMetadata,&RfReceiver::invoke,nullptr,nullptr}
};
#endif
