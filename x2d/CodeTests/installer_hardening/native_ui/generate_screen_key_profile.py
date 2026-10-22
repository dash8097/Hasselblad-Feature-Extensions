"""固定4.2 Keys共享附件/列表属性，无新增相机客户端。"""
import struct
def generate(b):
 sy={s.name:s for s in b.symbols}
 def addr(n):
  s=sy[n];assert s['st_value'] and s['st_size'];return s['st_value']
 meta=addr('_ZN18QQuickKeysAttached16staticMetaObjectE');_,st,d,*_=struct.unpack('<7Q',b.read(meta,56));h=struct.unpack('<14I',b.read(d,56))
 def text(i):o,n=struct.unpack('<II',b.read(st+8*i,8));return b.read(st+o,n).decode()
 props={}
 for i in range(h[6]):
  e=struct.unpack('<5I',b.read(d+4*(h[7]+5*i),20));props[text(e[0])]={'index':i,'type':text(e[1]&0x7fffffff) if e[1]&0x80000000 else e[1]}
 assert props['enabled']=={'index':0,'type':1};assert props['forwardTo']=={'index':1,'type':'QQmlListProperty<QQuickItem>'},props
 f={'attached_object':addr('_Z27qmlAttachedPropertiesObjectP7QObjectPFS0_S0_Eb'),'keys_call':addr('_ZN18QQuickKeysAttached18qt_static_metacallEP7QObjectN11QMetaObject4CallEiPPv'),'window':addr('_ZNK10QQuickItem6windowEv'),'active':addr('_ZNK12QQuickWindow15activeFocusItemEv'),'parent_item':addr('_ZNK10QQuickItem10parentItemEv')}
 out=['#pragma once','#include "screen_key_profile.hpp"','static const UiCode screen_key_functions[]={']
 out+=['{0x%xUL,{%s}},'%(a,','.join('0x%02x'%v for v in b.read(a,16))) for a in f.values()];out+=['};','static x2dui::ScreenKeyProfile screen_key_profile(uintptr_t base){x2dui::ScreenKeyProfile p{};p.key=key_profile(base);']
 out+=['p.%s=reinterpret_cast<decltype(p.%s)>(base+0x%xUL);'%(k,k,a) for k,a in f.items()];out+=['return p;}']
 return ('\n'.join(out)+'\n').encode(),dict(firmware='X2D 100C 4.2.0',properties=props,functions=f,shared_qt_attachment=True,foreign_keyhandler_created=False)
