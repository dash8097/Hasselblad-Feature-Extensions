"""固定4.2.0现存VM/Qt数据访问；仅原厂方法，不包含自有JS源码。"""
import struct,hashlib,re,sys
from pathlib import Path
def generate(b):
 assert hashlib.sha256(b.data).hexdigest()=='16391452abdc69de9e0807e065c0f4ab3f1ccb5fc288f6fc4e6f5cb3bdca12e0'
 sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'temporary_af_speed_probe'))
 from inspect_menu_resources import resources
 original=dict(resources(b))[':/app/qml/liveview/AfSymbol.qml'];state=re.search(r'enum State\s*\{([^}]+)',original.decode()).group(1);names=[x.strip() for x in state.split(',')];assert names[:4]==['None = 0','Idle','Failed','FoundFocus']
 sy={s.name:s for s in b.symbols}
 def addr(n):s=sy[n];assert s['st_value'] and s['st_size'];return s['st_value']
 def metadata(n):
  a=addr('_ZN'+n+'16staticMetaObjectE');_,st,d,*_=struct.unpack('<7Q',b.read(a,56));h=struct.unpack('<14I',b.read(d,56))
  def text(i):off,size=struct.unpack('<II',b.read(st+8*i,8));return b.read(st+off,size).decode()
  return d,h,text
 fields={}
 for cls,names,proxy in [('11CameraProxy',{'live_view_state':'liveState'},0),('11SystemProxy',{'screen_status':'screenStatus','screens_active':'screensActive'},1)]:
  d,h,t=metadata(cls)
  for i in range(h[6]):
   e=struct.unpack('<5I',b.read(d+4*(h[7]+i*5),20));name=t(e[0]);typ=t(e[1]&0x7fffffff) if e[1]&0x80000000 else e[1]
   if name not in names:continue
   width=4
   if isinstance(typ,str):
    found=[struct.unpack('<HHI',b.read(v['st_value'],8))[2] for n,v in sy.items() if n.startswith('_ZN9QtPrivate25QMetaTypeInterfaceWrapperI') and typ.split('::')[-1] in n and n.endswith('E8metaTypeE')];assert found and len(set(found))==1;width=found[0]
   assert width in (1,4);fields[names[name]]=dict(proxy=proxy,index=i,width=width)
 assert len(fields)==3
 d,h,t=metadata('9HblmTypes');enums={}
 for i in range(h[8]):
  e=struct.unpack('<5I',b.read(d+4*(h[9]+5*i),20))
  for j in range(e[3]):k,v=struct.unpack('<II',b.read(d+4*(e[4]+2*j),8));enums[t(k)]=v
 assert enums['E_LiveViewState_Active']==1 and enums['E_ScreenStatus_Active']==0 and enums['E_Screens_None']==0
 enum_names={'running':'E_AutoFocusStatus_Running','runningCont':'E_AutoFocusStatus_RunningCont','roiDefault':'E_AfRoiType_Default','roiFace':'E_AfRoiType_Face','roiLeft':'E_AfRoiType_EyeLeft','roiRight':'E_AfRoiType_EyeRight'}
 sentinel={}
 for n,value in [('_ZNK15CommonConstants22faceDetectionIdInvalidEv',-1),('_ZNK15CommonConstants30faceDetectionIdAfcLostTrackingEv',-2)]:
  a=addr(n);blob=b.read(a,8);assert blob.hex()==('00008012c0035fd6' if value==-1 else 'e07b1f32c0035fd6');sentinel[n]={'address':a,'bytes':blob.hex(),'value':value}
 data={}
 for k,typ,size in [('rectType','6QRectF',32),('pointType','7QPointF',16),('colorType','6QColor',16)]:
  a=addr('_ZN9QtPrivate25QMetaTypeInterfaceWrapperI'+typ+'E8metaTypeE');revision,alignment,actual=struct.unpack('<HHI',b.read(a,8));assert actual==size;data[k]=a
 data['listMeta']=addr('_ZN15VariantMapModel16staticMetaObjectE');data['cameraDbusMeta']=addr('_ZN15CameraProxyDbus16staticMetaObjectE')
 data['mapType']=addr('_ZN9QtPrivate25QMetaTypeInterfaceWrapperI4QMapI7QString8QVariantEE8metaTypeE')
 assert struct.unpack('<HHI',b.read(data['mapType'],8))[2]==8
 # Bind the actual original model metadata, including its QVariant return type.
 d,h,t=metadata('15VariantMapModel')
 methods=[struct.unpack('<6I',b.read(d+4*(h[5]+6*i),24)) for i in range(h[4])]
 get=next(e for e in methods if t(e[0])=='get')
 assert get[1]==1 and struct.unpack('<3I',b.read(d+4*get[2],12))[:2]==(41,2)
 vm=dict(resources(b))[':/app/qml/viewmodels/LiveviewViewModel.qml']
 assert b'readonly property QtObject faceRegions: Camera.all_face_regions' in vm
 f={k:addr(n) for k,n in [('list_get','_ZNK15VariantMapModel3getEi'),('map_value','_ZNK4QMapI7QString8QVariantE5valueERKS0_RKS1_'),('face_target','_ZN15CameraProxyDbus17doSet_face_targetEi')]}
 out=['#pragma once','#include "face_live_profile.hpp"','static const UiCode face_live_functions[]={'];out+=['{0x%xUL,{%s}},'%(a,','.join('0x%02x'%v for v in b.read(a,16))) for a in f.values()];out+=['};','static x2dui::FaceLiveProfile face_live_profile(uintptr_t base){x2dui::FaceLiveProfile p{};p.factory=face_factory_profile(base);']
 out+=['p.%s=reinterpret_cast<decltype(p.%s)>(base+0x%xUL);'%(k,k,a) for k,a in dict(data,**f).items()]
 out+=['p.%s={%d,%d,%d,false,false,false};'%(k,s['proxy'],s['index'],s['width']) for k,s in fields.items()]
 out+=['p.%s=%d;'%(k,enums[n]) for k,n in enum_names.items()];out+=['return p;}']
 return ('\n'.join(out)+'\n').encode(),dict(firmware='X2D 100C 4.2.0',fields=fields,data=data,functions=f,enums={k:enums[n] for k,n in enum_names.items()},js_evaluation=False,new_proxy=False,sentinel=sentinel,model_type="VariantMapModel",model_get_return="QVariant(QVariantMap)",model_vm_property="faceRegions",vm_sha256=hashlib.sha256(vm).hexdigest(),found_focus=3,found_focus_source_sha256=hashlib.sha256(original).hexdigest())
