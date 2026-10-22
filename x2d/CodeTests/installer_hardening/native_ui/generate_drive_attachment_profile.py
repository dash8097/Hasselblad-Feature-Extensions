"""固定第一代 X2D 4.2.0 的 Drive UI 元数据，只解析本地原厂 ELF。"""
import hashlib,struct,sys
from pathlib import Path
GUI_SHA='16391452abdc69de9e0807e065c0f4ab3f1ccb5fc288f6fc4e6f5cb3bdca12e0'
def generate(b):
 assert hashlib.sha256(b.data).hexdigest()==GUI_SHA
 sy={s.name:s for s in b.symbols}
 def address(n):s=sy[n];assert s['st_value'] and s['st_size'];return s['st_value']
 def enums(name):
  a=address(name);_,st,d,*_=struct.unpack('<7Q',b.read(a,56));h=struct.unpack('<14I',b.read(d,56))
  def text(i):o,n=struct.unpack('<II',b.read(st+i*8,8));return b.read(st+o,n).decode()
  result={}
  for i in range(h[8]):
   e=struct.unpack('<5I',b.read(d+4*(h[9]+i*5),20))
   for j in range(e[3]):k,v=struct.unpack('<II',b.read(d+4*(e[4]+j*2),8));result[text(k)]=v
  return result
 types=enums('_ZN9HblmTypes16staticMetaObjectE');keys=enums('_ZN10KeyHandler16staticMetaObjectE')
 modes=['Single','Continuous','SelfTimer','Interval','ExposureBracketing','FocusBracketing'];values=[types['E_DriveModes_'+s] for s in modes];assert values==list(range(6));assert types['E_ExpMode_FullAuto']==4 and types['E_CameraKeyOption_OverrideHalfPress']==2
 assert types['E_ExpMode_Manual']==0
 names={'method_index':'_ZNK11QMetaObject13indexOfMethodEPKc','metacall':'_ZN11QMetaObject8metacallEP7QObjectNS_4CallEiPPv'};f={k:address(v) for k,v in names.items()};assert f=={'method_index':0x8ca550,'metacall':0x8c9b88}
 sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'temporary_af_speed_probe'))
 from inspect_menu_resources import resources
 resources_=dict(resources(b));paths=[':/app/qml/controlscreen/ControlScreenViewModel.qml',':/app/qml/popups/PopoverDriveMode.qml',':/app/qml/components/DriveModeGrid.qml'];resource_hashes={p:hashlib.sha256(resources_[p]).hexdigest() for p in paths}
 model=resources_[paths[0]].decode();assert all('property string '+name+':' in model for name in ('aperture','shutterSpeed','isoAsString'))
 out=['#pragma once','#include "drive_attachment.hpp"','static const UiCode drive_attachment_functions[]={']
 out+=['{0x%xUL,{%s}},'%(a,','.join('0x%02x'%v for v in b.read(a,16))) for a in sorted(set(f.values()))];out+=['};','static x2dui::DriveAttachmentProfile drive_attachment_profile(uintptr_t base){x2dui::DriveAttachmentProfile p{};']
 out+=['p.%s=reinterpret_cast<decltype(p.%s)>(base+0x%xUL);'%(k,k,a) for k,a in f.items()]
 out+=['p.drive[%d]=%d;'%(i,v) for i,v in enumerate(values)];out+=['p.fullAuto=4;p.halfOverride=2;']
 mapping={'keyUp':['JstkUp'],'keyDown':['JstkDown'],'keyLeft':['Up','Left'],'keyRight':['Down','Right'],'keyAccept':['JstkCenter','PopAccept']}
 for field,ns in mapping.items():
  for i in range(2):out+=['p.%s[%d]=%d;'%(field,i,keys[ns[i]] if i<len(ns) else -1)]
 out+=['p.keyParameters=%d;return p;}'%keys['Enter']]
 return ('\n'.join(out)+'\n').encode(),dict(firmware='X2D 100C 4.2.0',gui_sha256=GUI_SHA,functions=f,original_resources=resource_hashes,drive=dict(zip(modes,values)),fullAuto=4,halfOverride=2,keymap={field:[keys[n] for n in ns] for field,ns in mapping.items()},keyParameters=keys['Enter'],exposure_properties=['aperture','shutterSpeed','isoAsString'],new_proxy_created=False)
