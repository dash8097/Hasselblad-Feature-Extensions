#include "qt_key_profile.hpp"
#include <QtQuick/private/qquickitem_p.h>
#include <QtQuick/private/qquickevents_p_p.h>
#include <QLibrary>
#include <cstdlib>
template<class F>static F fn(const char*n,const char*library="Qt6Quick"){auto p=QLibrary::resolve(library,n);if(!p)std::abort();return reinterpret_cast<F>(p);}
x2dui::KeyProfile qt_key_profile(){x2dui::KeyProfile p{};
 p.attached=fn<decltype(p.attached)>("_ZN18QQuickKeysAttached21qmlAttachedPropertiesEP7QObject");p.complete=fn<decltype(p.complete)>("_ZN18QQuickKeysAttached17componentCompleteEv");p.destroy=[](void*o){delete static_cast<QObject*>(o);};p.cached=fn<decltype(p.cached)>("_Z27qmlAttachedPropertiesObjectP7QObjectPFS0_S0_Eb","Qt6Qml");p.event=reinterpret_cast<x2dui::AbiMetaCall>(QQuickKeyEvent::staticMetaObject.d.static_metacall);p.cast=[](const void*m,const void*o)->const void*{return static_cast<const QMetaObject*>(m)->cast(static_cast<const QObject*>(o));};p.meta_event=&QQuickKeyEvent::staticMetaObject;return p;}
