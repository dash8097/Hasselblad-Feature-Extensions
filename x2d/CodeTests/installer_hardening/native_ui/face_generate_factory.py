"""4.2.0 固定现存 Camera/System 字段、Face popup 列表；不创建代理。"""
import struct
def generate(b):
 sy={s.name:s for s in b.symbols}
 def addr(n):s=sy[n];assert s['st_value'] and s['st_size'];return s['st_value']
 def metadata(n):
  a=addr('_ZN'+n+'16staticMetaObjectE');_,st,d,*_=struct.unpack('<7Q',b.read(a,56));h=struct.unpack('<14I',b.read(d,56))
  def text(i):off,size=struct.unpack('<II',b.read(st+8*i,8));return b.read(st+off,size).decode()
  return d,h,text
 fields={}
 for cls,names,proxy in [('11CameraProxy',{'face_detection':'face'},0),('11SystemProxy',{'debug_mode':'master','debug_options':'options'},1)]:
  d,h,t=metadata(cls)
  for i in range(h[6]):
   e=struct.unpack('<5I',b.read(d+4*(h[7]+i*5),20));name=t(e[0]);typ=t(e[1]&0x7fffffff) if e[1]&0x80000000 else e[1]
   if name not in names:continue
   width=1 if typ==1 else 4
   if isinstance(typ,str):
    found=[]
    for name2,s in sy.items():
     if name2.startswith('_ZN9QtPrivate25QMetaTypeInterfaceWrapperI') and typ.split('::')[-1] in name2 and name2.endswith('E8metaTypeE'):found.append(struct.unpack('<HHI',b.read(s['st_value'],8))[2])
    assert found and len(set(found))==1;width=found[0]
   assert e[2]&2 and width in (1,4)
   fields[names[name]]=dict(proxy=proxy,index=i,width=width,boolean=typ==1,type=typ,name=name)
 assert set(fields)=={'face','master','options'} and fields['master']['boolean'] and fields['options']['width']==4
 d,h,t=metadata('9HblmTypes');enums={}
 for i in range(h[8]):
  e=struct.unpack('<5I',b.read(d+4*(h[9]+5*i),20))
  for j in range(e[3]):key,value=struct.unpack('<II',b.read(d+4*(e[4]+2*j),8));enums[t(key)]=value
 modes={k:enums[k] for k in ['E_FaceDetection_Off','E_FaceDetection_Multi','E_DebugOption_EyeDetection']};assert modes['E_DebugOption_EyeDetection'] and not modes['E_DebugOption_EyeDetection']&(modes['E_DebugOption_EyeDetection']-1)
 d,h,t=metadata('10KeyHandler');keyvals={}
 for i in range(h[8]):
  e=struct.unpack('<5I',b.read(d+4*(h[9]+5*i),20))
  if t(e[0])=='KeyFn':
   for j in range(e[3]):key,value=struct.unpack('<II',b.read(d+4*(e[4]+2*j),8));keyvals[t(key)]=value
 expected={'Up':27,'NavLeft':15,'Down':26,'NavRight':14,'PopAccept':28,'FaceDetection':86,'Abort':6,'Menu':1,'Browse':61};assert all(keyvals[k]==v for k,v in expected.items())
 f={k:addr(n) for k,n in [('list_construct','_ZN17QQmlListReferenceC1ERK8QVariant'),('list_destroy','_ZN17QQmlListReferenceD1Ev'),('list_count','_ZNK17QQmlListReference5countEv'),('list_at','_ZNK17QQmlListReference2atEx'),('method_index','_ZNK11QMetaObject13indexOfMethodEPKc'),('metacall','_ZN11QMetaObject8metacallEP7QObjectNS_4CallEiPPv')]}
 out=['#pragma once','#include "face_factory.hpp"','static const UiCode face_factory_functions[]={']
 out+=['{0x%xUL,{%s}},'%(a,','.join('0x%02x'%v for v in b.read(a,16))) for a in f.values()];out+=['};','static x2dui::FaceFactoryProfile face_factory_profile(uintptr_t base){x2dui::FaceFactoryProfile p{};p.proxy=factory_proxy_profile(base);']
 for k,s in fields.items():out+=['p.%s={%d,%d,%d,%s,false,true};'%(k,s['proxy'],s['index'],s['width'],str(s['boolean']).lower())]
 out+=['p.modes={%d,%d,%d};'%(modes['E_FaceDetection_Off'],modes['E_FaceDetection_Multi'],modes['E_DebugOption_EyeDetection'])]
 out+=['p.%s=reinterpret_cast<decltype(p.%s)>(base+0x%xUL);'%(k,k,a) for k,a in f.items()];out+=['return p;}']
 return ('\n'.join(out)+'\n').encode(),dict(firmware='X2D 100C 4.2.0',fields=fields,enums=modes,functions=f,keys=expected,new_proxy_created=False)
