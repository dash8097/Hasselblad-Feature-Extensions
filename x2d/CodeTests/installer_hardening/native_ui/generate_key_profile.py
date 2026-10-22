"""固定4.2.0原厂Keys/KeyFn ABI，仅读已解析ELF。"""
import struct
def generate(b):
 symbols={s.name:s for s in b.symbols}
 def address(n):
  s=symbols[n];assert s['st_value'] and s['st_size'];return s['st_value']
 names={'attached':'_ZN18QQuickKeysAttached21qmlAttachedPropertiesEP7QObject','complete':'_ZN18QQuickKeysAttached17componentCompleteEv','event':'_ZN14QQuickKeyEvent18qt_static_metacallEP7QObjectN11QMetaObject4CallEiPPv','cast':'_ZNK11QMetaObject4castEPK7QObject','destroy':'_ZN18QQuickKeysAttachedD0Ev'}
 names['cached']='_Z27qmlAttachedPropertiesObjectP7QObjectPFS0_S0_Eb'
 f={k:address(n) for k,n in names.items()};meta=address('_ZN14QQuickKeyEvent16staticMetaObjectE')
 def metadata(a):
  _,strings,data,*_=struct.unpack('<7Q',b.read(a,56));h=struct.unpack('<14I',b.read(data,56))
  def text(i):o,n=struct.unpack('<II',b.read(strings+8*i,8));return b.read(strings+o,n).decode()
  return data,h,text
 data,h,text=metadata(meta);props={}
 for i in range(h[6]):ent=struct.unpack('<5I',b.read(data+4*(h[7]+i*5),20));props[text(ent[0])]=i
 assert all(props[k]==v for k,v in {'key':0,'modifiers':2,'isAutoRepeat':3,'accepted':6}.items())
 handler_meta=address('_ZN10KeyHandler16staticMetaObjectE');data,h,text=metadata(handler_meta);enums={}
 for i in range(h[8]):
  en=struct.unpack('<5I',b.read(data+4*(h[9]+i*5),20))
  if text(en[0])=='KeyFn':
   for j in range(en[3]):k,v=struct.unpack('<II',b.read(data+4*(en[4]+2*j),8));enums[text(k)]=v
 required={'Menu':1,'Escape':5,'Abort':6,'Select':8,'HalfPress':11,'PopAccept':28,'JstkEscape':31};assert all(enums[k]==v for k,v in required.items())
 slot=address('_ZZN17KeyHandlerForeign8instanceEvE9sInstance');provider=address('_ZN17KeyHandlerForeign8instanceEv')
 # Provider reads a single static existing instance; no provider call is made.
 # Bind its source implementation too, because construction has side effects.
 assert slot==0x21bc838 and provider==0x33adb0
 focus={'window':address('_ZNK10QQuickItem6windowEv'),'active':address('_ZNK12QQuickWindow15activeFocusItemEv'),'focus':address('_ZN10QQuickItem16forceActiveFocusEN2Qt11FocusReasonE')}
 mapping={'is_fn':address('_ZNK14KeyHandlerBase4isFnEijN10KeyHandler5KeyFnE'),'unblockable':address('_ZNK14KeyHandlerBase25isUnblockableCustomButtonEij'),'cast':f['cast']}
 out=['#pragma once','#include "key_filter.hpp"','#include "key_mapping.hpp"','#include "processing_host.hpp"','static const UiCode key_functions[]={']
 out+=['{0x%xUL,{%s}},'%(a,','.join('0x%02x'%v for v in b.read(a,16))) for a in sorted(set(f.values())|set(mapping.values())|set(focus.values())|{provider})];out+=['};','static const uintptr_t key_slot=0x%xUL,key_event_meta=0x%xUL,key_handler_meta=0x%xUL;'%(slot,meta,handler_meta),'static x2dui::KeyProfile key_profile(uintptr_t base){x2dui::KeyProfile p{};']
 out+=['p.%s=reinterpret_cast<decltype(p.%s)>(base+0x%xUL);'%(k,k,a) for k,a in f.items()];out+=['p.meta_event=reinterpret_cast<void*>(base+key_event_meta);return p;}','static x2dui::KeyMapProfile key_map_profile(uintptr_t base){x2dui::KeyMapProfile p{};']
 out+=['p.%s=reinterpret_cast<decltype(p.%s)>(base+0x%xUL);'%(k,k,a) for k,a in mapping.items()];out+=['p.instance=reinterpret_cast<void*const*>(base+key_slot);p.meta=reinterpret_cast<void*>(base+key_handler_meta);return p;}']
 out+=['static x2dui::FocusProfile focus_profile(uintptr_t base){x2dui::FocusProfile p{};']
 out+=['p.%s=reinterpret_cast<decltype(p.%s)>(base+0x%xUL);'%(k,k,a) for k,a in focus.items()];out+=['return p;}']
 return ('\n'.join(out)+'\n').encode(),{'functions':f,'event_meta':meta,'event_properties':props,'keyfn_required':required,'keyfn_all':enums,'focus_functions':focus,'map_functions':mapping,'existing_instance_storage':slot,'provider_readonly_evidence':{'address':provider,'size':152,'not_invoked':True},'creates_keyhandler':False}
