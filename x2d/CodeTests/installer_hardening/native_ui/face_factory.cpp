#include "face_factory.hpp"
#include <cmath>
#include <cstring>
namespace x2dui {namespace {
void*object(const FaceFactoryProfile&p,const FactoryFieldSpec&s){if(s.proxy>=3||!p.proxy.slots[s.proxy]||!p.proxy.call[s.proxy]||!p.proxy.cast)return nullptr;auto*o=*p.proxy.slots[s.proxy];return o?const_cast<void*>(p.proxy.cast(p.proxy.meta[s.proxy],o)):nullptr;}
bool read(const FaceFactoryProfile&p,const FactoryFieldSpec&s,uint32_t&x){auto*o=object(p,s);if(!o||!(s.width==1||s.width==2||s.width==4))return false;bool bit=false;x=0;int status=-1,flags=0;void*a[]={s.boolean?static_cast<void*>(&bit):&x,nullptr,&status,&flags};p.proxy.call[s.proxy](o,1,s.index,a);if(s.boolean)x=bit;return true;}
bool number(const StockProfile&s,const MenuProfile&m,void*o,const char*n,double&x){if(!o||!stock_has(s,o,n))return false;auto v=m.property(o,n);bool ok=false;x=m.number(&v,&ok);m.variant_destroy(&v);return ok&&std::isfinite(x);}
void free_string(const MenuProfile&p,AbiArray&a){if(a.owner){auto*r=static_cast<int*>(a.owner);if(__atomic_load_n(r,__ATOMIC_RELAXED)>=0&&__atomic_sub_fetch(r,1,__ATOMIC_ACQ_REL)==0)p.deallocate(a.owner,2,8);}}
}
bool face_factory_snapshot(const FaceFactoryProfile&p,FaceModeSnapshot&out){out={};uint32_t f=0,m=0,o=0;if(!read(p,p.face,f)||!read(p,p.master,m)||!read(p,p.options,o))return false;out={true,m!=0,o,int(f)};return true;}
bool face_factory_apply(const FaceFactoryProfile&p,const FaceAction&a){FactoryFieldSpec field;switch(a.kind){case FaceActionKind::FaceDetection:if(a.value!=p.modes.off&&a.value!=p.modes.multi)return false;field=p.face;break;case FaceActionKind::DebugMaster:if(a.value!=0&&a.value!=1)return false;field=p.master;break;case FaceActionKind::DebugOptions:if(a.value<0||uint64_t(a.value)>UINT32_MAX)return false;field=p.options;break;default:return false;}
 auto*o=object(p,field);if(!o||!field.writable)return false;bool bit=a.value!=0;uint32_t n=uint32_t(a.value);int status=-1,flags=0;void*args[]={field.boolean?static_cast<void*>(&bit):&n,nullptr,&status,&flags};p.proxy.call[field.proxy](o,2,field.index,args);return true;
}
bool face_factory_popup(const FaceFactoryProfile&p,const StockProfile&s,const MenuProfile&m,void*o,FacePopupSnapshot&out,char16_t*heading,size_t cap){
 out={};if(!o||!heading||cap<2||!p.list_construct||!p.list_destroy||!p.list_count||!p.list_at||!stock_has(s,o,"faceDetectionModel")||!stock_has(s,o,"heading"))return false;double can=0,raw=0;if(!number(s,m,o,"canChangeFaceDetection",can)||(can!=0&&can!=1)||!number(s,m,o,"faceDetection",raw)||raw<0||raw>255||double(int(raw))!=raw)return false;
 auto hv=m.property(o,"heading");auto text=m.string(&hv);bool valid=text.size>=0&&size_t(text.size)<cap&&(text.data||!text.size);if(valid){auto*t=static_cast<const char16_t*>(text.data);for(std::ptrdiff_t i=0;i<text.size;i++)if(!t[i])valid=false;if(valid){if(text.size)std::memcpy(heading,t,size_t(text.size)*2);heading[text.size]=0;}}free_string(m,text);m.variant_destroy(&hv);if(!valid)return false;
 auto v=s.property(o,"faceDetectionModel");alignas(8)void*list=nullptr;p.list_construct(&list,&v);auto count=p.list_count(&list);bool multi=false,off=false,multiSeen=false,offSeen=false;valid=count>=0&&count<=64;
 for(std::ptrdiff_t i=0;valid&&i<count;i++){auto*item=p.list_at(&list,i);double mode=0,allow=0;valid=number(s,m,item,"mode",mode)&&number(s,m,item,"canSelect",allow)&&(allow==0||allow==1)&&mode>=0&&mode<=255&&double(int(mode))==mode;if(!valid)break;if(int(mode)==p.modes.multi){if(multiSeen)valid=false;multiSeen=true;multi=allow!=0;}if(int(mode)==p.modes.off){if(offSeen)valid=false;offSeen=true;off=allow!=0;}}
 p.list_destroy(&list);s.variant_destroy(&v);if(!valid)return false;out.attached=true;out.canChange=can!=0;out.rawMode=int(raw);out.multiAllowed=multiSeen&&multi;out.offAllowed=offSeen&&off;return true;
}
bool face_factory_close_popup(const FaceFactoryProfile&p,const StockProfile&s,void*o){if(!o||!p.method_index||!p.metacall)return false;auto*table=*static_cast<const void**>(o);if(!table)return false;auto*fn=*static_cast<const void*const*>(table);if(s.valid_vtable&&!s.valid_vtable(s.validation_context,table,fn))return false;auto*meta=reinterpret_cast<const void*(*)(const void*)>(reinterpret_cast<uintptr_t>(fn))(o);int index=p.method_index(meta,"closePopup()");if(index<0)return false;void*args[]={nullptr};p.metacall(o,0,index,args);return true;}
}
