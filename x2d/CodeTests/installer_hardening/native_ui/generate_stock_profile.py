"""固定4.2.0原厂qrc工厂ABI，输出地址/摘要，不输出原厂资源内容。"""
import struct,hashlib
def generate(b,resources):
 symbols={s.name:s for s in b.symbols}
 def address(n):
  s=symbols[n];assert s['st_value'] and s['st_size'];return s['st_value']
 names={'engine':'_Z9qmlEnginePK7QObject','context':'_ZNK10QQmlEngine11rootContextEv','url_construct':'_ZN4QUrlC1ERK7QStringNS_11ParsingModeE','url_destroy':'_ZN4QUrlD1Ev','component_construct':'_ZN13QQmlComponentC1EP10QQmlEngineRK4QUrlNS_15CompilationModeEP7QObject','component_destroy':'_ZN13QQmlComponentD0Ev','status':'_ZNK13QQmlComponent6statusEv','create':'_ZN13QQmlComponent6createEP11QQmlContext','cast':'_ZNK11QMetaObject4castEPK7QObject','parent':'_ZN7QObject9setParentEPS_','visual_parent':'_ZN10QQuickItem13setParentItemEPS_','variant_bool':'_ZN8QVariantC1Eb','variant_int':'_ZN8QVariantC1Ei','variant_double':'_ZN8QVariantC1Ed','variant_string':'_ZN8QVariantC1ERK7QString','variant_destroy':'_ZN8QVariantD1Ev','set':'_ZN7QObject11setPropertyEPKcRK8QVariant','property_index':'_ZNK11QMetaObject15indexOfPropertyEPKc','property':'_ZNK7QObject8propertyEPKc','variant_convert':'_ZN8QVariant7convertE9QMetaType','variant_data':'_ZN8QVariant4dataEv','variant_typed':'_ZN8QVariantC1E9QMetaTypePKv'}
 names.update(qml_property_construct='_ZN12QQmlPropertyC1EP7QObjectRK7QString',qml_property_write='_ZNK12QQmlProperty5writeERK8QVariant',qml_property_destroy='_ZN12QQmlPropertyD1Ev')
 names.update(string_ctor='_ZN7QStringC1EPK5QCharx',string_deallocate='_ZN10QArrayData10deallocateEPS_xx')
 functions={k:address(v) for k,v in names.items()};functions['allocate']=0x2194e8
 sizes={}
 for n in ['13QQmlComponent','4QUrl','8QVariant','P7QObject']:
  a=address('_ZN9QtPrivate25QMetaTypeInterfaceWrapperI'+n+'E8metaTypeE');revision,align,size=struct.unpack('<HHI',b.read(a,8));assert revision==0 and align==8;sizes[n]=size
 assert sizes=={'13QQmlComponent':16,'4QUrl':8,'8QVariant':32,'P7QObject':8}
 size_type=address('_ZN9QtPrivate25QMetaTypeInterfaceWrapperI5QSizeE8metaTypeE');assert struct.unpack('<HHI',b.read(size_type,8))==(0,4,8)
 meta_image=address('_ZN11QQuickImage16staticMetaObjectE')
 qobject_pointer=address('_ZN9QtPrivate25QMetaTypeInterfaceWrapperIP7QObjectE8metaTypeE');meta_item=address('_ZN10QQuickItem16staticMetaObjectE')
 urls=[':/app/qml/exposescreen/ExposeScreen.qml',':/app/qml/exposescreen/ExposeIconText.qml',':/app/qml/exposescreen/ExposeProgress.qml',':/app/qml/viewmodels/ExposeScreenViewModel.qml',':/app/qml/components/TextValueRow.qml',':/app/qml/mainmenu/MenuHeader.qml',':/app/qml/components/Photo.qml',':/app/qml/components/buttons/FramedImage.qml',':/app/qml/mainmenu/SettingsGeneric.qml',':/app/qml/components/TiledImage.qml',':/app/qml/mainmenu/MenuBoolSelector.qml',':/app/qml/popups/PopupIconText.qml',':/app/qml/popups/Popover.qml',':/app/qml/components/buttons/FramedItem.qml',':/app/qml/components/Touchpad.qml']
 hashes={u:hashlib.sha256(resources[u]).hexdigest() for u in urls}
 # The first three QObject virtuals are metaObject/metacast/metacall, then
 # complete and deleting destructors. Validate original vtable evidence.
 vtable=address('_ZTV7QObject');slots=struct.unpack('<5Q',b.read(vtable+16,40));assert slots[0]==address('_ZNK7QObject10metaObjectEv') and slots[4]==address('_ZN7QObjectD0Ev')
 out=['#pragma once','#include "stock_factory.hpp"','static const UiCode stock_functions[]={']
 out+=['{0x%xUL,{%s}},'%(a,','.join('0x%02x'%v for v in b.read(a,16))) for a in sorted(set(functions.values()))]
 out+=['};','static const ObjectData stock_data[]={{0x%xUL,72},{0x%xUL,56},{0x%xUL,72},{0x%xUL,56}};'%(qobject_pointer,meta_item,size_type,meta_image),'static x2dui::StockProfile stock_profile(uintptr_t base){x2dui::StockProfile p{};']
 out+=['p.%s=reinterpret_cast<decltype(p.%s)>(base+0x%xUL);'%(k,k,a) for k,a in functions.items()]
 out+=['p.size_type=reinterpret_cast<const void*>(base+0x%xUL);p.meta_image=reinterpret_cast<const void*>(base+0x%xUL);'%(size_type,meta_image)]
 out+=['p.component_size=16;p.meta_item=reinterpret_cast<const void*>(base+0x%xUL);p.qobject_pointer_type=reinterpret_cast<const void*>(base+0x%xUL);return p;}'%(meta_item,qobject_pointer)]
 return ('\n'.join(out)+'\n').encode(),{'functions':functions,'sizes':sizes,'resource_sha256':hashes,'qobject_virtuals':slots,'qt_original_resources_distributed':False}
