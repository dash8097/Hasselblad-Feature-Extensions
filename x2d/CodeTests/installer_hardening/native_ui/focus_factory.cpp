#include "focus_factory.hpp"
#include <cmath>
#include <cstring>
namespace x2dui {namespace {
bool number(const StockProfile&s,const MenuProfile&m,void*o,const char*n,double&x){if(!o||!stock_has(s,o,n))return false;auto v=m.property(o,n);bool ok=false;x=m.number(&v,&ok);m.variant_destroy(&v);return ok&&std::isfinite(x);}
void release(const MenuProfile&p,AbiArray&a){if(a.owner){auto*r=static_cast<int*>(a.owner);if(__atomic_load_n(r,__ATOMIC_RELAXED)>=0&&__atomic_sub_fetch(r,1,__ATOMIC_ACQ_REL)==0)p.deallocate(a.owner,2,8);}}
}
bool focus_factory_snapshot(const FocusFactoryProfile&p,const StockProfile&s,const MenuProfile&m,void*o,void*model,FocusSnapshot&out,char16_t*heading,size_t cap){
 out={};const auto&f=p.common;if(!o||!model||!heading||cap<2||!f.list_construct||!f.list_destroy||!f.list_count||!f.list_at||!stock_has(s,o,"focusModeModel")||!stock_has(s,o,"heading"))return false;
 double selectable=0,mode=0;if(!number(s,m,model,"focusModeSelectable",selectable)||(selectable!=0&&selectable!=1)||!number(s,m,o,"focusMode",mode)||mode<0||mode>255||std::floor(mode)!=mode)return false;
 auto hv=m.property(o,"heading");auto text=m.string(&hv);bool valid=text.size>=0&&size_t(text.size)<cap&&(text.data||!text.size);if(valid){auto*t=static_cast<const char16_t*>(text.data);for(std::ptrdiff_t i=0;i<text.size;i++)if(!t[i])valid=false;if(valid){if(text.size)std::memcpy(heading,t,size_t(text.size)*2);heading[text.size]=0;}}release(m,text);m.variant_destroy(&hv);if(!valid)return false;
 auto v=s.property(o,"focusModeModel");alignas(8)void*list=nullptr;f.list_construct(&list,&v);auto count=f.list_count(&list);bool seen[3]{},allowed[3]{};valid=count>=0&&count<=64;
 for(std::ptrdiff_t i=0;valid&&i<count;i++){auto*item=f.list_at(&list,i);double value=0,allow=0;valid=number(s,m,item,"focusMode",value)&&number(s,m,item,"valid",allow)&&(allow==0||allow==1)&&value>=0&&value<=255&&std::floor(value)==value;if(!valid)break;if(value>=1&&value<=3){unsigned index=unsigned(value)-1;if(seen[index])valid=false;seen[index]=true;allowed[index]=allow!=0;}}
 f.list_destroy(&list);s.variant_destroy(&v);if(!valid||!seen[0]||!seen[2])return false;
 // The old extension enabled the fixed, otherwise hardcoded false AFC gate.
 // Native menu does this locally; the original MF-ring/can-change guard above
 // remains mandatory. No global singleton or factory model is rewritten.
 allowed[1]=true;
 const auto&proxy=f.proxy;if(!proxy.slots[1]||!proxy.call[1]||!proxy.cast||!(p.languageWidth==1||p.languageWidth==2||p.languageWidth==4))return false;
 void*object=*proxy.slots[1];if(!object||!proxy.cast(proxy.meta[1],object))return false;uint32_t language=0;int status=-1,flags=0;void*args[]={&language,nullptr,&status,&flags};proxy.call[1](object,1,int(p.languageIndex),args);if(language>255)return false;
 out.valid=true;out.selectable=selectable!=0;out.mode=int(mode);out.language=int(language);for(unsigned i=0;i<3;i++)out.allowed[i]=allowed[i];return true;
}
bool focus_factory_select(const FocusFactoryProfile&p,const StockProfile&s,void*o,int mode){
 if(!o||mode<1||mode>3||!p.common.method_index||!p.common.metacall)return false;auto*table=*static_cast<const void**>(o);if(!table)return false;auto*fn=*static_cast<const void*const*>(table);if(s.valid_vtable&&!s.valid_vtable(s.validation_context,table,fn))return false;
 auto*meta=reinterpret_cast<const void*(*)(const void*)>(reinterpret_cast<uintptr_t>(fn))(o);int index=p.common.method_index(meta,"setFocusMode(int)");if(index<0)return false;void*args[]={nullptr,&mode};p.common.metacall(o,0,index,args);return true;
}
}
