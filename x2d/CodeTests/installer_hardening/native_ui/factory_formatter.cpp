#include "factory_formatter.hpp"
namespace x2dui {
bool factory_format(const FactoryFormatterProfile&p,const MenuProfile&m,const UiText&name,const AbiVariant&value,bool alternate,UiText&out){
 if(!p.slot||!*p.slot||!p.meta||!p.cast||!p.call||!p.string_ctor||!p.string_deallocate||!m.deallocate||!name.valid()||name.empty()||!p.cast(p.meta,*p.slot))return false;
 AbiString field=abi_owned_string(p,name.data(),static_cast<std::ptrdiff_t>(name.size()));AbiArray result{};void*args[]={&result,&field,const_cast<AbiVariant*>(&value)};
 p.call(*p.slot,0,alternate?7:6,args);abi_release_string(p,field);bool valid=result.size>=0&&result.size<=4096&&(result.data||!result.size);
 UiText text;if(valid)text=UiText(static_cast<const char16_t*>(result.data),static_cast<size_t>(result.size));
 if(result.owner){auto*r=static_cast<int*>(result.owner);if(__atomic_load_n(r,__ATOMIC_RELAXED)>=0&&__atomic_sub_fetch(r,1,__ATOMIC_ACQ_REL)==0)m.deallocate(result.owner,2,8);}
 if(!valid||!text.valid())return false;out=static_cast<UiText&&>(text);return true;
}
}
