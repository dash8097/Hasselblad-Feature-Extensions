"""第一代4.2.0显示设置固定ABI；正常原厂formatter调用，禁止任意字段。"""
import struct
def generate(b):
 sy={s.name:s for s in b.symbols}
 def address(n):
  s=sy[n];assert s['st_value'] and s['st_size'];return s['st_value']
 def metadata(name):
  a=address('_ZN'+name+'16staticMetaObjectE');_,st,d,*_=struct.unpack('<7Q',b.read(a,56));h=struct.unpack('<14I',b.read(d,56))
  def txt(i):off,n=struct.unpack('<II',b.read(st+8*i,8));return b.read(st+off,n).decode()
  return d,h,txt
 d,h,t=metadata('14ValueFormatter');methods=[]
 for i,n,expect in [(4,'getVariableListModel',[11,10]),(5,'setSettingValue',[43,'GuiTypes::E_Proxy',10,10]),(6,'getDisplayValue',[10,10,41]),(9,'stringTranslated',[10,10,10])]:
  e=struct.unpack('<6I',b.read(d+4*(h[5]+6*i),24));types=struct.unpack('<'+str(e[1]+1)+'I',b.read(d+4*e[2],4*(e[1]+1)));types=[t(v&0x7fffffff) if v&0x80000000 else v for v in types];assert t(e[0])==n and types==expect,(n,types);methods.append(dict(index=i,name=n,types=types))
 d,h,t=metadata('11CameraProxy');e=struct.unpack('<5I',b.read(d+4*(h[7]+11*5),20));assert t(e[0])=='allow_variable_fps' and e[1]==1 and e[2]&2
 enums={}
 for name,wanted in [('9HblmTypes',{'E_Screens','E_CameraKeyOption'}),('8GuiTypes',{'E_Proxy'})]:
  d,h,t=metadata(name)
  for i in range(h[8]):
   e=struct.unpack('<5I',b.read(d+4*(h[9]+i*5),20))
   if t(e[0]) in wanted:
    for j in range(e[3]):key,value=struct.unpack('<II',b.read(d+4*(e[4]+j*2),8));enums[t(key)]=value
 expected={'E_Screens_Back':1,'E_Screens_EVF':2,'E_CameraKeyOption_OverrideHalfPress':2,'E_Proxy_System':4,'E_Proxy_Camera':2};assert all(enums[k]==v for k,v in expected.items())
 destroy=address('_ZN5QListI7QStringED2Ev')
 out=['#pragma once','#include "screen_factory_profile.hpp"','static const UiCode screen_functions[]={','{0x%xUL,{%s}},'%(destroy,','.join('0x%02x'%v for v in b.read(destroy,16))),'};',
 'static x2dui::ScreenFactoryProfile screen_factory_profile(uintptr_t base){x2dui::ScreenFactoryProfile p{};p.proxy=factory_proxy_profile(base);p.formatter=formatter_profile(base);',
 'p.list_destroy=reinterpret_cast<decltype(p.list_destroy)>(base+0x%xUL);'%destroy,
 'p.backBit=1;p.evfBit=2;p.halfBit=2;p.systemProxy=4;p.cameraProxy=2;return p;}']
 return ('\n'.join(out)+'\n').encode(),{'firmware':'X2D 100C 4.2.0','formatter_methods':methods,'fps_bool_property_index':11,'enums':expected,'list_destructor':destroy,'new_proxy_created':False,'arbitrary_setting_names':False}
