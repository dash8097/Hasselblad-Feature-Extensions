"""原厂4.2.0取景/参数页入口及被动触摸观察；不合成快门。"""
import struct
def generate(b):
 sy={s.name:s for s in b.symbols};f={}
 for key,name in [('index','_ZNK11QMetaObject13indexOfMethodEPKc'),('metacall','_ZN11QMetaObject8metacallEP7QObjectNS_4CallEiPPv'),('point_construct','_ZN18QQuickPointHandlerC1EP10QQuickItem'),('point_complete','_ZN20QQuickPointerHandler17componentCompleteEv'),('point_property','_ZN20QQuickPointerHandler18qt_static_metacallEP7QObjectN11QMetaObject4CallEiPPv')]:
  s=sy[name];assert s['st_value'] and s['st_size'];f[key]=s['st_value']
 out=['#pragma once','#include "screen_mode_attachment.hpp"','static const UiCode screen_live_functions[]={']
 out+=['{0x%xUL,{%s}},'%(a,','.join('0x%02x'%v for v in b.read(a,16))) for a in f.values()];out+=['};','static x2dui::ScreenLiveProfile screen_live_profile(uintptr_t base){x2dui::ScreenLiveProfile p{};']
 _,strings,table,*_=struct.unpack('<7Q',b.read(sy['_ZN20QQuickPointerHandler16staticMetaObjectE']['st_value'],56));header=struct.unpack('<14I',b.read(table,56))
 prop=struct.unpack('<5I',b.read(table+4*(header[7]+2*5),20));offset,length=struct.unpack('<II',b.read(strings+8*prop[0],8));assert b.read(strings+offset,length)==b'target' and prop[2]&2
 point=sy['_ZN9QtPrivate25QMetaTypeInterfaceWrapperI18QQuickPointHandlerE8metaTypeE']['st_value'];assert struct.unpack('<HHI',b.read(point,8))==(0,8,32)
 mapping=sy['_ZN9QtPrivate25QMetaTypeInterfaceWrapperI4QMapI7QString8QVariantEE8metaTypeE']['st_value'];assert struct.unpack('<HHI',b.read(mapping,8))==(0,8,8)
 _,st,d,*_=struct.unpack('<7Q',b.read(sy['_ZN9HblmTypes16staticMetaObjectE']['st_value'],56));h=struct.unpack('<14I',b.read(d,56))
 def text(i):off,n=struct.unpack('<II',b.read(st+8*i,8));return b.read(st+off,n).decode()
 active=None
 for i in range(h[8]):
  e=struct.unpack('<5I',b.read(d+4*(h[9]+5*i),20))
  if text(e[0])=='E_LiveViewState':
   for j in range(e[3]):
    k,v=struct.unpack('<II',b.read(d+4*(e[4]+2*j),8))
    if text(k)=='E_LiveViewState_Active':active=v
 assert active==1
 out+=['p.%s=reinterpret_cast<decltype(p.%s)>(base+0x%xUL);'%(k,k,a) for k,a in f.items()];out+=['p.point_size=32;p.map_type=reinterpret_cast<const void*>(base+0x%xUL);p.liveview_active=1;'%mapping,'return p;}']
 return ('\n'.join(out)+'\n').encode(),dict(firmware='X2D 100C 4.2.0',functions=f,methods=['setLiveView(bool)','toggle_screen(QString,QVariant)'],point_size=32,map_size=8,map_type=mapping,liveview_active=active,camera_shutter_or_af=False)
