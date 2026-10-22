// Test-only resolver: function calls below enter the real installed Qt DLLs.
#include "qt_abi_profile.hpp"
#include <QLibrary>
#include <QQuickItem>
#include <QtQuick/private/qquickrectangle_p.h>
#include <QtQuick/private/qquicktext_p.h>
#include <QtQuick/private/qquickmousearea_p.h>
#include <QtQuick/private/qquickevents_p_p.h>
#include <QFont>
#include <cstdio>
#include <cstdlib>
#include <new>
namespace {
void*symbol(const char*module,const char*name){auto p=QLibrary::resolve(module,name);if(!p){std::fprintf(stderr,"ABI_SYMBOL_MISSING %s\n",name);std::exit(2);}return reinterpret_cast<void*>(p);}
void*allocate(std::size_t n){return ::operator new(n);}
template<class F>F function(const char*module,const char*name){return reinterpret_cast<F>(symbol(module,name));}
}
x2dui::AbiViewProfile qt_abi_profile(){
 using namespace x2dui;AbiViewProfile p{};p.allocate=allocate;
 const char*ctors[]={"_ZN10QQuickItemC1EPS_","_ZN15QQuickRectangleC1EP10QQuickItem","_ZN10QQuickTextC1EP10QQuickItem","_ZN15QQuickMouseAreaC1EP10QQuickItem"};
 unsigned sizes[]={sizeof(QQuickItem),sizeof(QQuickRectangle),sizeof(QQuickText),sizeof(QQuickMouseArea)};
 for(int i=0;i<4;i++){p.construct[i]=function<void(*)(void*,void*)>("Qt6Quick",ctors[i]);p.sizes[i]=sizes[i];p.complete[i]=function<void(*)(void*)>("Qt6Quick",i==2?"_ZN10QQuickText17componentCompleteEv":"_ZN10QQuickItem17componentCompleteEv");}
 p.destroy=function<void(*)(void*)>("Qt6Quick","_ZN10QQuickItemD0Ev");p.parent=function<void(*)(void*,void*)>("Qt6Core","_ZN7QObject9setParentEPS_");
 p.item=reinterpret_cast<AbiMetaCall>(QQuickItem::staticMetaObject.d.static_metacall);p.text=reinterpret_cast<AbiMetaCall>(QQuickText::staticMetaObject.d.static_metacall);p.rectangle=reinterpret_cast<AbiMetaCall>(QQuickRectangle::staticMetaObject.d.static_metacall);p.mouse_event=reinterpret_cast<AbiMetaCall>(QQuickMouseEvent::staticMetaObject.d.static_metacall);
 p.set_name=function<decltype(p.set_name)>("Qt6Core","_ZN7QObject13setObjectNameE14QAnyStringView");p.font_ctor=function<decltype(p.font_ctor)>("Qt6Gui","_ZN5QFontC1Ev");p.font_size=function<decltype(p.font_size)>("Qt6Gui","_ZN5QFont12setPixelSizeEi");p.font_dtor=function<decltype(p.font_dtor)>("Qt6Gui","_ZN5QFontD1Ev");
 p.map_scene=function<decltype(p.map_scene)>("Qt6Quick","_ZNK10QQuickItem10mapToSceneERK7QPointF");p.map_from_scene=function<decltype(p.map_from_scene)>("Qt6Quick","_ZNK10QQuickItem12mapFromSceneERK7QPointF");p.connect=function<decltype(p.connect)>("Qt6Core","_ZN7QObject11connectImplEPKS_PPvS1_S3_PN9QtPrivate15QSlotObjectBaseEN2Qt14ConnectionTypeEPKiPK11QMetaObject");p.connection_dtor=function<decltype(p.connection_dtor)>("Qt6Core","_ZN11QMetaObject10ConnectionD1Ev");
 p.meta_object=&QObject::staticMetaObject;p.meta_item=&QQuickItem::staticMetaObject;p.meta_mouse=&QQuickMouseArea::staticMetaObject;
 const char*signal_functions[]={"_ZN15QQuickMouseArea7clickedEP16QQuickMouseEvent","_ZN15QQuickMouseArea7pressedEP16QQuickMouseEvent","_ZN15QQuickMouseArea15positionChangedEP16QQuickMouseEvent","_ZN15QQuickMouseArea8releasedEP16QQuickMouseEvent","_ZN7QObject9destroyedEPS_","_ZN10QQuickItem14visibleChangedEv"};
 for(int i=0;i<6;i++)p.signal_functions[i]=symbol(i==4?"Qt6Core":"Qt6Quick",signal_functions[i]);p.string_ctor=function<decltype(p.string_ctor)>("Qt6Core","_ZN7QStringC1EPK5QCharx");p.string_deallocate=function<decltype(p.string_deallocate)>("Qt6Core","_ZN10QArrayData10deallocateEPS_xx");return p;
}
