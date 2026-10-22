"""自有三项菜单的固定4.2.0枚举/系统语言/原厂焦点信号证据。"""
import struct,hashlib
def generate(b,resources):
 sy={s.name:s for s in b.symbols}
 def metadata(name):
  a=sy['_ZN'+name+'16staticMetaObjectE']['st_value'];_,st,d,*_=struct.unpack('<7Q',b.read(a,56));h=struct.unpack('<14I',b.read(d,56))
  def text(i):off,n=struct.unpack('<II',b.read(st+8*i,8));return b.read(st+off,n).decode()
  return d,h,text
 d,h,t=metadata('9HblmTypes');enums={}
 for i in range(h[8]):
  e=struct.unpack('<5I',b.read(d+4*(h[9]+5*i),20))
  for j in range(e[3]):k,v=struct.unpack('<II',b.read(d+4*(e[4]+j*2),8));enums[t(k)]=v
 expected={'E_FocusModes_Afs':1,'E_FocusModes_Afc':2,'E_FocusModes_Man':3,'E_GuiPopupState_FocusMode':4,'E_CameraKeyOption_OverrideHalfPress':2}
 assert all(enums[k]==v for k,v in expected.items())
 languages=['English','Spanish','French','German','Italian','Swedish','Russian','Japanese','SimplifiedChinese','TraditionalChinese','Korean'];assert all(enums['E_Language_'+n]==i for i,n in enumerate(languages))
 d,h,t=metadata('11SystemProxy');e=struct.unpack('<5I',b.read(d+4*(h[7]+20*5),20));assert t(e[0])=='language_index' and e[1]==36
 d,h,t=metadata('10KeyHandler');keys={}
 for i in range(h[8]):
  e=struct.unpack('<5I',b.read(d+4*(h[9]+5*i),20))
  if t(e[0])=='KeyFn':
   for j in range(e[3]):k,v=struct.unpack('<II',b.read(d+4*(e[4]+j*2),8));keys[t(k)]=v
 assert keys['FocusMode']==57
 files=[':/app/qml/controlscreen/ControlScreen.qml',':/app/qml/controlscreen/ControlScreenViewModel.qml',':/app/qml/proxies/CameraUI.qml',':/app/qml/popups/PopoverFocusMode.qml']
 assert b'signal setFocusMode(int mode)' in resources[files[-1]] and b'function onSetFocusMode(mode) { viewModel.setFocusMode(mode) }' in resources[files[0]]
 assert b'readonly property bool canChangeAfc: false' in resources[files[2]]
 out='#pragma once\n#include "focus_factory.hpp"\nstatic x2dui::FocusFactoryProfile focus_factory_profile(uintptr_t base){x2dui::FocusFactoryProfile p{};p.common=face_factory_profile(base);p.languageIndex=20;p.languageWidth=2;return p;}\n'
 return out.encode(),dict(firmware='X2D 4.2.0',enums=expected,languages=languages,focus_key=57,system_language_property_index=20,system_language_meta_type=36,resources={n:hashlib.sha256(resources[n]).hexdigest() for n in files},source_resources_distributed=False,afc_availability='local native selector; preserve original focusModeSelectable',global_canChangeAfc_modified=False)
