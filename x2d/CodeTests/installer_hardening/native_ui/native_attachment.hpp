#pragma once
#include "effect_preferences.hpp"
#include <QtCore/QPointer>
// ABI prototype: the caller must already execute in the existing GUI runtime.
// No static constructor, extra QGuiApplication, QML loader, or vendor method invocation.
class NativeUiAttachment final: public QObject {
 Q_OBJECT
 QPointer<QQuickItem> item;
 explicit NativeUiAttachment(QQuickItem*host):QObject(host){}
public:
 enum Result { Attached=0, AlreadyAttached=1, WrongThread=2, InvalidHost=3, AmbiguousHost=4, FactoryFailed=5, NotAttached=6 };
 using Factory=QQuickItem*(*)(QQuickItem*,bool);
 static Result attach(QQuickItem*host,bool networkEnabled,Factory factory=x2d_create_effect_preferences);
 static Result detach(QQuickItem*host);
 static Result discoverAndAttach(bool networkEnabled);
 ~NativeUiAttachment() override;
};
extern "C" Q_DECL_EXPORT int x2d_native_attach_v1(QQuickItem*,bool);
extern "C" Q_DECL_EXPORT int x2d_native_detach_v1(QQuickItem*);
extern "C" Q_DECL_EXPORT int x2d_native_discover_v1(bool);
