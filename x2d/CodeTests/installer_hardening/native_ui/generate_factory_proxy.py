"""固定4.2.0只借用既有Camera/System/Storage代理的属性ABI证据。"""
import struct
def generate(b):
 symbols={s.name:s for s in b.symbols}
 def a(n):s=symbols[n];assert s['st_value'] and s['st_size'];return s['st_value']
 classes=['11CameraProxy','11SystemProxy','12StorageProxy'];slots=[a('_ZN10GuiProxies13m_cameraProxyE'),a('_ZN10GuiProxies13m_systemProxyE'),a('_ZN10GuiProxies14m_storageProxyE')]
 metas=[a('_ZN'+n+'16staticMetaObjectE') for n in classes];calls=[a('_ZN'+n+'18qt_static_metacallEP7QObjectN11QMetaObject4CallEiPPv') for n in classes];cast=a('_ZNK11QMetaObject4castEPK7QObject')
 specs=[(0,'image_format',100,False,False),(0,'drive_mode',46,False,True),(0,'exit_drive_mode',54,False,True),(0,'forward_input_events',88,False,True),(0,'custom_option_liveview_evf_only',38,True,True),(1,'screen_idle_timeout',30,False,True),(1,'screens_active',32,False,False),(1,'screens_allowed',33,False,True),(2,'browse_active',11,True,False),(2,'capture_storage_mode',0,False,False)]
 specs.append((0,'exp_mode',56,False,False))
 specs.extend([(0,'image_bit_depth',99,False,False),(0,'tv_current',171,False,False),(0,'tv_min',174,False,False),(0,'tv_max',173,False,False),(0,'eshutter_current',51,True,False),(0,'exposure_status',65,False,False)])
 tables=[]
 for meta in metas:
  _,strings,data,*_=struct.unpack('<7Q',b.read(meta,56));h=struct.unpack('<14I',b.read(data,56));table={}
  def text(i):off,n=struct.unpack('<II',b.read(strings+8*i,8));return b.read(strings+off,n).decode()
  for i in range(h[6]):
   e=struct.unpack('<5I',b.read(data+4*(h[7]+i*5),20));t=text(e[1]&0x7fffffff) if e[1]&0x80000000 else e[1];table[text(e[0])]={'index':i,'type':t,'flags':e[2]}
  tables.append(table)
 evidence=[]
 for proxy,name,index,boolean,writable in specs:
  prop=tables[proxy][name];assert prop['index']==index and (prop['type']==1 if boolean else prop['type']!=1)
  if isinstance(prop['type'],str):
   bare=prop['type'].split('::')[-1];found=[]
   for n,s in symbols.items():
    if n.startswith('_ZN9QtPrivate25QMetaTypeInterfaceWrapperI') and bare in n and n.endswith('E8metaTypeE'):
     _,align,size=struct.unpack('<HHI',b.read(s['st_value'],8));found.append((align,size))
   assert found and len(set(found))==1 and found[0][1] in (1,2,4),(name,prop['type'],found)
   prop['width']=found[0][1]
  else:prop['width']=1 if boolean else 4
  if writable:assert prop['flags']&2,(name,'not writable')
  evidence.append({'proxy':classes[proxy],'property':name,**prop,'boolean':boolean,'signed':prop['type']==2,'allowed_write':writable})
 out=['#pragma once','#include "factory_proxy.hpp"','static const UiCode factory_proxy_functions[]={']
 out+=['{0x%xUL,{%s}},'%(x,','.join('0x%02x'%v for v in b.read(x,16))) for x in sorted(set(calls+[cast]))];out+=['};','static const uintptr_t factory_proxy_slots[]={'+','.join('0x%xUL'%v for v in slots)+'};','static const uintptr_t factory_proxy_metas[]={'+','.join('0x%xUL'%v for v in metas)+'};','static x2dui::FactoryProxyProfile factory_proxy_profile(uintptr_t base){x2dui::FactoryProxyProfile p{};']
 for i in range(3):out+=['p.slots[%d]=reinterpret_cast<void*const*>(base+0x%xUL);p.meta[%d]=reinterpret_cast<void*>(base+0x%xUL);p.call[%d]=reinterpret_cast<x2dui::AbiMetaCall>(base+0x%xUL);'%(i,slots[i],i,metas[i],i,calls[i])]
 out+=['p.cast=reinterpret_cast<decltype(p.cast)>(base+0x%xUL);'%cast]
 for i,(proxy,name,index,boolean,writable) in enumerate(specs):out+=['p.fields[%d]={%d,%d,%d,%s,%s,%s};'%(i,proxy,index,evidence[i]['width'],str(boolean).lower(),str(evidence[i]['signed']).lower(),str(writable).lower())]
 out+=['return p;}'];return ('\n'.join(out)+'\n').encode(),{'slots':slots,'metas':metas,'calls':calls,'properties':evidence,'new_proxy_created':False}
