#include "qt_menu_profile.hpp"
#include <QLibrary>
#include <QQuickItem>
#include <QtQuick/private/qquickitemview_p.h>
#include <cstdio>
#include <cstdlib>
namespace {
template<class F>F resolve(const char*module,const char*name){auto p=QLibrary::resolve(module,name);if(!p){std::fprintf(stderr,"MENU_ABI_SYMBOL_MISSING %s\n",name);std::exit(2);}return reinterpret_cast<F>(p);}
}
x2dui::MenuProfile qt_menu_profile(){using namespace x2dui;MenuProfile p{};
 p.children=resolve<decltype(p.children)>("Qt6Quick","_ZNK10QQuickItem10childItemsEv");p.name=resolve<decltype(p.name)>("Qt6Core","_ZNK7QObject10objectNameEv");p.property=resolve<decltype(p.property)>("Qt6Core","_ZNK7QObject8propertyEPKc");p.string=resolve<decltype(p.string)>("Qt6Core","_ZNK8QVariant8toStringEv");p.number=resolve<decltype(p.number)>("Qt6Core","_ZNK8QVariant8toDoubleEPb");p.variant_destroy=resolve<decltype(p.variant_destroy)>("Qt6Core","_ZN8QVariantD1Ev");p.deallocate=resolve<decltype(p.deallocate)>("Qt6Core","_ZN10QArrayData10deallocateEPS_xx");
 p.cast=resolve<decltype(p.cast)>("Qt6Core","_ZNK11QMetaObject4castEPK7QObject");p.meta_item=&QQuickItem::staticMetaObject;p.meta_itemview=&QQuickItemView::staticMetaObject;p.itemview=reinterpret_cast<AbiMetaCall>(QQuickItemView::staticMetaObject.d.static_metacall);
 p.content=resolve<decltype(p.content)>("Qt6Quick","_ZNK15QQuickFlickable11contentItemEv");p.content_height=resolve<decltype(p.content_height)>("Qt6Quick","_ZNK15QQuickFlickable13contentHeightEv");p.bottom_margin=resolve<decltype(p.bottom_margin)>("Qt6Quick","_ZNK15QQuickFlickable12bottomMarginEv");p.set_bottom_margin=resolve<decltype(p.set_bottom_margin)>("Qt6Quick","_ZN15QQuickFlickable15setBottomMarginEd");return p;
}
