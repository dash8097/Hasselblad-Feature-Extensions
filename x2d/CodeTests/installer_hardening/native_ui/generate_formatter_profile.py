"""仅固定4.2.0已有ValueFormatter只读方法，不创建单例。"""
import struct
def generate(b):
 symbols={s.name:s for s in b.symbols}
 def a(n):
  s=symbols[n];assert s['st_value'] and s['st_size'];return s['st_value']
 slot=a('_ZN14ValueFormatter10m_instanceE');meta=a('_ZN14ValueFormatter16staticMetaObjectE');call=a('_ZN14ValueFormatter18qt_static_metacallEP7QObjectN11QMetaObject4CallEiPPv');cast=a('_ZNK11QMetaObject4castEPK7QObject')
 _,strings,data,*_=struct.unpack('<7Q',b.read(meta,56));h=struct.unpack('<14I',b.read(data,56))
 def text(i):
  offset,n=struct.unpack('<II',b.read(strings+8*i,8));return b.read(strings+offset,n).decode()
 ctor=a('_ZN7QStringC1EPK5QCharx');deallocate=a('_ZN10QArrayData10deallocateEPS_xx')
 evidence=[]
 for index,name in ((6,'getDisplayValue'),(7,'getDisplayAltValue')):
  e=struct.unpack('<6I',b.read(data+4*(h[5]+6*index),24));assert text(e[0])==name and e[1]==2
  types=struct.unpack('<3I',b.read(data+4*e[2],12));assert types==(10,10,41),(name,types)
  evidence.append({'index':index,'method':name,'types':types})
 out=['#pragma once','#include "factory_formatter.hpp"','static const UiCode formatter_functions[]={']
 for addr in sorted({call,cast,ctor,deallocate}):out+=['{0x%xUL,{%s}},'%(addr,','.join('0x%02x'%v for v in b.read(addr,16)))]
 out+=['};','static const ObjectData formatter_data[]={{0x%xUL,8},{0x%xUL,56}};'%(slot,meta),'static x2dui::FactoryFormatterProfile formatter_profile(uintptr_t base){x2dui::FactoryFormatterProfile p{};','p.slot=reinterpret_cast<void*const*>(base+0x%xUL);p.meta=reinterpret_cast<const void*>(base+0x%xUL);p.call=reinterpret_cast<x2dui::AbiMetaCall>(base+0x%xUL);p.cast=reinterpret_cast<decltype(p.cast)>(base+0x%xUL);return p;}'%(slot,meta,call,cast)]
 out[-1]=out[-1].replace('return p;', 'p.string_ctor=reinterpret_cast<decltype(p.string_ctor)>(base+0x%xUL);p.string_deallocate=reinterpret_cast<decltype(p.string_deallocate)>(base+0x%xUL);return p;'%(ctor,deallocate))
 return ('\n'.join(out)+'\n').encode(),{'slot':slot,'meta':meta,'call':call,'string_ctor':ctor,'string_deallocate':deallocate,'methods':evidence,'creates_instance':False,'setter_included':False}
