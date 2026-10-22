"""固定4.2.0既有ContentModel入口；不构造singleton，不调用拍摄接口。"""
import struct
def generate(b):
 sy={s.name:s for s in b.symbols}
 def a(n):s=sy[n];assert s['st_value'] and s['st_size'];return s['st_value']
 slot=a('_ZZN19ContentModelForeign6createEP10QQmlEngineP9QJSEngineE5model');assert sy['_ZZN19ContentModelForeign6createEP10QQmlEngineP9QJSEngineE5model']['st_size']==8
 meta=a('_ZN12ContentModel16staticMetaObjectE')
 _,st,d,*_=struct.unpack('<7Q',b.read(meta,56));h=struct.unpack('<14I',b.read(d,56));props={}
 def text(i):o,n=struct.unpack('<II',b.read(st+i*8,8));return b.read(st+o,n).decode()
 for i in range(h[6]):e=struct.unpack('<5I',b.read(d+4*(h[7]+i*5),20));props[text(e[0])]=(i,e[1])
 assert props['path']==(0,10) and props['isBrowsing']==(1,1)
 names={'cast':'_ZNK11QMetaObject4castEPK7QObject','path_changed':'_ZN12ContentModel11pathChangedERK7QString','enter_dir':'_ZN12ContentModel8enterDirERK7QString','update_loading':'_ZN12ContentModel13updateLoadingEv'}
 funcs={k:a(v) for k,v in names.items()}
 out=['#pragma once','#include "album_adapter.hpp"','static const UiCode album_functions[]={']
 out+=['{0x%xUL,{%s}},'%(v,','.join('0x%02x'%c for c in b.read(v,16))) for v in sorted(set(funcs.values()))]
 out+=['};','static const uintptr_t album_slot=0x%xUL,album_meta=0x%xUL;'%(slot,meta),'static x2dui::AlbumProfile album_profile(uintptr_t base){x2dui::AlbumProfile p{};p.slot=reinterpret_cast<void*const*>(base+album_slot);p.meta=reinterpret_cast<void*>(base+album_meta);']
 out+=['p.%s=reinterpret_cast<decltype(p.%s)>(base+0x%xUL);'%(k,k,v) for k,v in funcs.items()]
 out+=['return p;}'];return ('\n'.join(out)+'\n').encode(),{'functions':funcs,'slot':slot,'meta':meta,'properties':props,'constructs_singleton':False}
