"""仅从固定 4.2.0 元对象证据生成本模块适配表；不操作设备。"""
import struct
def generate(b):
 symbols={s.name:s for s in b.symbols}
 def address(name):
  s=symbols[name];assert s['st_value'] and s['st_size'];return s['st_value']
 names={
 'destroy':'_ZN10QQuickItemD0Ev','parent':'_ZN7QObject9setParentEPS_',
 'item':'_ZN10QQuickItem18qt_static_metacallEP7QObjectN11QMetaObject4CallEiPPv',
 'rectangle':'_ZN15QQuickRectangle18qt_static_metacallEP7QObjectN11QMetaObject4CallEiPPv',
 'text':'_ZN10QQuickText18qt_static_metacallEP7QObjectN11QMetaObject4CallEiPPv',
 'mouse_event':'_ZN16QQuickMouseEvent18qt_static_metacallEP7QObjectN11QMetaObject4CallEiPPv',
 'set_name':'_ZN7QObject13setObjectNameE14QAnyStringView','font_ctor':'_ZN5QFontC1Ev','font_size':'_ZN5QFont12setPixelSizeEi','font_dtor':'_ZN5QFontD1Ev',
 'map_scene':'_ZNK10QQuickItem10mapToSceneERK7QPointF',
 'map_from_scene':'_ZNK10QQuickItem12mapFromSceneERK7QPointF',
 'connect':'_ZN7QObject11connectImplEPKS_PPvS1_S3_PN9QtPrivate15QSlotObjectBaseEN2Qt14ConnectionTypeEPKiPK11QMetaObject',
 'connection_dtor':'_ZN11QMetaObject10ConnectionD1Ev'}
 ctors=['_ZN10QQuickItemC1EPS_','_ZN15QQuickRectangleC1EP10QQuickItem','_ZN10QQuickTextC1EP10QQuickItem','_ZN15QQuickMouseAreaC1EP10QQuickItem']
 complete=['_ZN10QQuickItem17componentCompleteEv']*4;complete[2]='_ZN10QQuickText17componentCompleteEv'
 signals=['_ZN15QQuickMouseArea7clickedEP16QQuickMouseEvent','_ZN15QQuickMouseArea7pressedEP16QQuickMouseEvent','_ZN15QQuickMouseArea15positionChangedEP16QQuickMouseEvent','_ZN15QQuickMouseArea8releasedEP16QQuickMouseEvent','_ZN7QObject9destroyedEPS_','_ZN10QQuickItem14visibleChangedEv']
 metas={'meta_object':'_ZN7QObject16staticMetaObjectE','meta_item':'_ZN10QQuickItem16staticMetaObjectE','meta_mouse':'_ZN15QQuickMouseArea16staticMetaObjectE'}
 # Verify operator-new trampoline against the actual relocation, not a guessed import.
 rel=b.elf.get_section_by_name('.rela.plt');dyn=b.elf.get_section(rel['sh_link']);new=[r['r_offset'] for r in rel.iter_relocations() if dyn.get_symbol(r['r_info_sym']).name=='_Znwm'];assert new==[0x213ef78]
 code=list(b.cs.disasm(b.read(0x2194e8,16),0x2194e8));assert [i.mnemonic for i in code]==['adrp','ldr','add','br'] and '#0xf78' in code[1].op_str
 names.update(string_ctor='_ZN7QStringC1EPK5QCharx',string_deallocate='_ZN10QArrayData10deallocateEPS_xx')
 functions={'allocate':0x2194e8,**{k:address(v) for k,v in names.items()}}
 for i,n in enumerate(ctors):functions['construct[%d]'%i]=address(n)
 for i,n in enumerate(complete):functions['complete[%d]'%i]=address(n)
 for i,n in enumerate(signals):functions['signal_functions[%d]'%i]=address(n)
 cast=address('_ZNK11QMetaObject4castEPK7QObject');all_functions=sorted(set(functions.values())|{cast})
 menu_names={'children':'_ZNK10QQuickItem10childItemsEv','name':'_ZNK7QObject10objectNameEv','property':'_ZNK7QObject8propertyEPKc','string':'_ZNK8QVariant8toStringEv','number':'_ZNK8QVariant8toDoubleEPb','variant_destroy':'_ZN8QVariantD1Ev','deallocate':'_ZN10QArrayData10deallocateEPS_xx','cast':'_ZNK11QMetaObject4castEPK7QObject','itemview':'_ZN14QQuickItemView18qt_static_metacallEP7QObjectN11QMetaObject4CallEiPPv','content':'_ZNK15QQuickFlickable11contentItemEv','content_height':'_ZNK15QQuickFlickable13contentHeightEv','bottom_margin':'_ZNK15QQuickFlickable12bottomMarginEv','set_bottom_margin':'_ZN15QQuickFlickable15setBottomMarginEd'}
 menu_functions={k:address(v) for k,v in menu_names.items()}
 window_functions={'windows':'_ZN15QGuiApplication10allWindowsEv','root':'_ZNK12QQuickWindow11contentItemEv'}
 window_functions={k:address(v) for k,v in window_functions.items()}
 signal_names={'index':'_ZNK11QMetaObject13indexOfSignalEPKc','method':'_ZNK11QMetaObject6methodEi','signal_index':'_ZN18QMetaObjectPrivate11signalIndexERK11QMetaMethod','connect':'_ZN14QObjectPrivate7connectEPK7QObjectiS2_PN9QtPrivate15QSlotObjectBaseEN2Qt14ConnectionTypeE','disconnect':'_ZN7QObject10disconnectERKN11QMetaObject10ConnectionE','connection_dtor':'_ZN11QMetaObject10ConnectionD1Ev'}
 signal_functions={k:address(v) for k,v in signal_names.items()}
 all_functions=sorted(set(all_functions)|set(menu_functions.values())|set(window_functions.values())|set(signal_functions.values()))
 data={k:address(v) for k,v in metas.items()}
 menu_data={'meta_item':address('_ZN10QQuickItem16staticMetaObjectE'),'meta_itemview':address('_ZN14QQuickItemView16staticMetaObjectE')}
 window_meta=address('_ZN12QQuickWindow16staticMetaObjectE')
 variant=address('_ZN9QtPrivate25QMetaTypeInterfaceWrapperI8QVariantE8metaTypeE');assert struct.unpack('<HHI',b.read(variant,8))==(0,8,32)
 sizes=[]
 for n in ['10QQuickItem','15QQuickRectangle','10QQuickText','15QQuickMouseArea','5QFont']:
  a=address('_ZN9QtPrivate25QMetaTypeInterfaceWrapperI'+n+'E8metaTypeE');revision,align,size=struct.unpack('<HHI',b.read(a,8));assert revision==0 and align==8;sizes.append(size)
 assert sizes==[32,32,40,32,16]
 def props(meta):
  _,strings,table,*_=struct.unpack('<7Q',b.read(meta,56));header=struct.unpack('<14I',b.read(table,56));result={}
  for i in range(header[6]):
   entry=struct.unpack('<5I',b.read(table+4*(header[7]+5*i),20));off,n=struct.unpack('<II',b.read(strings+8*entry[0],8));result[b.read(strings+off,n).decode()]=i
  return result
 tables={'item':props(data['meta_item']),'rectangle':props(address('_ZN15QQuickRectangle16staticMetaObjectE')),'text':props(address('_ZN10QQuickText16staticMetaObjectE')),'mouse':props(address('_ZN16QQuickMouseEvent16staticMetaObjectE'))}
 required={'item':{'x':4,'y':5,'z':6,'width':7,'height':8,'opacity':9,'enabled':10,'visible':11,'clip':27},'rectangle':{'color':0},'text':{'text':0,'font':1,'color':2,'horizontalAlignment':6,'verticalAlignment':8},'mouse':{'y':1}}
 tables['itemview']=props(menu_data['meta_itemview']);required['itemview']={'footer':15,'footerItem':16}
 for kind,entries in required.items():
  for name,index in entries.items():assert tables[kind][name]==index,(kind,name)
 out=['#pragma once','#include "abi_view_api.hpp"','#include "menu_discovery.hpp"','#include "signal_bridge.hpp"','#include <type_traits>','static const UiCode object_functions[]={']
 out+=['{0x%xUL,{%s}},'%(a,','.join('0x%02x'%v for v in b.read(a,16))) for a in all_functions];out+=['};','struct ObjectData{uintptr_t address;size_t size;};','static const ObjectData object_data[]={']
 out+=['{0x%xUL,56},'%a for a in sorted(set(data.values())|set(menu_data.values())|{window_meta})];out+=['};','static const uintptr_t object_cast=0x%xUL,object_meta_item=0x%xUL;'%(cast,data['meta_item']),'static x2dui::AbiViewProfile object_profile(uintptr_t base){x2dui::AbiViewProfile p{};']
 out+=['p.%s=reinterpret_cast<std::remove_reference_t<decltype(p.%s)>>(base+0x%xUL);'%(key,key,a) for key,a in functions.items()]
 out+=['p.%s=reinterpret_cast<const void*>(base+0x%xUL);'%(key,a) for key,a in data.items()]
 out+=['p.sizes[%d]=%d;'%(i,v) for i,v in enumerate(sizes[:4])];out+=['return p;}']
 out+=['static x2dui::MenuProfile menu_profile(uintptr_t base){x2dui::MenuProfile p{};']
 out+=['p.%s=reinterpret_cast<decltype(p.%s)>(base+0x%xUL);'%(k,k,a) for k,a in menu_functions.items()]
 out+=['p.%s=reinterpret_cast<const void*>(base+0x%xUL);'%(k,a) for k,a in menu_data.items()]
 out+=['return p;}','static const uintptr_t window_meta=0x%xUL;'%window_meta]
 out+=['static const uintptr_t window_%s=0x%xUL;'%(k,a) for k,a in window_functions.items()]
 out+=['static x2dui::SignalProfile signal_profile(uintptr_t base){x2dui::SignalProfile p{};']
 out+=['p.%s=reinterpret_cast<decltype(p.%s)>(base+0x%xUL);'%(k,k,a) for k,a in signal_functions.items()]
 out+=['return p;}']
 return ('\n'.join(out)+'\n').encode(),{'functions':functions,'data':data,'sizes':sizes,'properties':tables,'required_properties':required,'menu_functions':menu_functions,'menu_data':menu_data,'window_functions':window_functions,'window_meta':window_meta,'variant_size':32,'signal_functions':signal_functions,'signal_index_contract':'QObjectPrivate::connect method index, internal conversion at 0x8fb584..0x8fb5e4'}
