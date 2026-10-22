#include "qt_stock_profile.hpp"
#include <QLibrary>
#include <QQuickItem>
#include <QQmlComponent>
#include <QQmlProperty>
#include <QSize>
#include <QtQuick/private/qquickimage_p.h>
#include <QUrl>
#include <QVariant>
#include <new>
#include <cstdlib>
template<class F>static F fn(const char*lib,const char*n){auto p=QLibrary::resolve(lib,n);if(!p)std::abort();return reinterpret_cast<F>(p);}
static void destroy(void*p){delete static_cast<QObject*>(p);}
static void*allocate(std::size_t n){return ::operator new(n);}
x2dui::StockProfile qt_stock_profile(){x2dui::StockProfile p{};static_assert(sizeof(QUrl)==8&&sizeof(QVariant)==32,"stock value ABI");p.allocate=allocate;p.component_size=sizeof(QQmlComponent);
 p.engine=fn<decltype(p.engine)>("Qt6Qml","_Z9qmlEnginePK7QObject");p.context=fn<decltype(p.context)>("Qt6Qml","_ZNK10QQmlEngine11rootContextEv");
 p.url_construct=fn<decltype(p.url_construct)>("Qt6Core","_ZN4QUrlC1ERK7QStringNS_11ParsingModeE");p.url_destroy=fn<decltype(p.url_destroy)>("Qt6Core","_ZN4QUrlD1Ev");
 p.component_construct=fn<decltype(p.component_construct)>("Qt6Qml","_ZN13QQmlComponentC1EP10QQmlEngineRK4QUrlNS_15CompilationModeEP7QObject");p.component_destroy=fn<decltype(p.component_destroy)>("Qt6Qml","_ZN13QQmlComponentD0Ev");p.status=fn<decltype(p.status)>("Qt6Qml","_ZNK13QQmlComponent6statusEv");p.create=fn<decltype(p.create)>("Qt6Qml","_ZN13QQmlComponent6createEP11QQmlContext");
 p.cast=fn<decltype(p.cast)>("Qt6Core","_ZNK11QMetaObject4castEPK7QObject");p.meta_item=&QQuickItem::staticMetaObject;
 p.parent=fn<decltype(p.parent)>("Qt6Core","_ZN7QObject9setParentEPS_");p.visual_parent=fn<decltype(p.visual_parent)>("Qt6Quick","_ZN10QQuickItem13setParentItemEPS_");p.object_destroy=destroy;
 p.variant_bool=fn<decltype(p.variant_bool)>("Qt6Core","_ZN8QVariantC1Eb");p.variant_int=fn<decltype(p.variant_int)>("Qt6Core","_ZN8QVariantC1Ei");p.variant_double=fn<decltype(p.variant_double)>("Qt6Core","_ZN8QVariantC1Ed");p.variant_string=fn<decltype(p.variant_string)>("Qt6Core","_ZN8QVariantC1ERK7QString");p.variant_destroy=fn<decltype(p.variant_destroy)>("Qt6Core","_ZN8QVariantD1Ev");p.set=fn<decltype(p.set)>("Qt6Core","_ZN7QObject11setPropertyEPKcRK8QVariant");p.property_index=fn<decltype(p.property_index)>("Qt6Core","_ZNK11QMetaObject15indexOfPropertyEPKc");p.property=fn<decltype(p.property)>("Qt6Core","_ZNK7QObject8propertyEPKc");p.variant_convert=fn<decltype(p.variant_convert)>("Qt6Core","_ZN8QVariant7convertE9QMetaType");p.variant_data=fn<decltype(p.variant_data)>("Qt6Core","_ZN8QVariant4dataEv");p.variant_typed=fn<decltype(p.variant_typed)>("Qt6Core","_ZN8QVariantC1E9QMetaTypePKv");p.qobject_pointer_type=QMetaType::fromType<QObject*>().iface();p.size_type=QMetaType::fromType<QSize>().iface();p.meta_image=&QQuickImage::staticMetaObject;static_assert(sizeof(QQmlProperty)==8&&sizeof(QSize)==8,"QML property/size ABI");p.qml_property_construct=fn<decltype(p.qml_property_construct)>("Qt6Qml","_ZN12QQmlPropertyC1EP7QObjectRK7QString");p.qml_property_write=fn<decltype(p.qml_property_write)>("Qt6Qml","_ZNK12QQmlProperty5writeERK8QVariant");p.qml_property_destroy=fn<decltype(p.qml_property_destroy)>("Qt6Qml","_ZN12QQmlPropertyD1Ev");p.string_ctor=fn<decltype(p.string_ctor)>("Qt6Core","_ZN7QStringC1EPK5QCharx");p.string_deallocate=fn<decltype(p.string_deallocate)>("Qt6Core","_ZN10QArrayData10deallocateEPS_xx");return p;
}
