"""固定4.2.0绑定所有权ABI；不生成或求值任何业务脚本。"""
def generate(b):
 symbols={s.name:s for s in b.symbols}
 names={'take':'_ZN14QQmlAnyBinding8takeFromERK12QQmlProperty','current':'_ZN14QQmlAnyBinding10ofPropertyERK12QQmlProperty','install':'_ZN14QQmlAnyBinding9installOnERK12QQmlPropertyNS_15InterceptorModeE','assign':'_ZN14QQmlAnyBindingaSERKS_','read':'_ZNK12QQmlProperty4readEv','valid':'_ZNK12QQmlProperty7isValidEv','writable':'_ZNK12QQmlProperty10isWritableEv','equal':'_ZNK8QVariant6equalsERKS_'}
 addresses={k:symbols[n]['st_value'] for k,n in names.items()};assert all(addresses.values())
 out=['#pragma once','#include "binding_lease.hpp"','static const UiCode binding_functions[]={']
 out+=['{0x%xUL,{%s}},'%(a,','.join('0x%02x'%v for v in b.read(a,16))) for a in sorted(set(addresses.values()))]
 out+=['};','static x2dui::BindingProfile binding_profile(uintptr_t base){x2dui::BindingProfile p{};']
 out+=['p.%s=reinterpret_cast<decltype(p.%s)>(base+0x%xUL);'%(k,k,a) for k,a in addresses.items()]
 out+=['return p;}'];return ('\n'.join(out)+'\n').encode(),{'functions':addresses,'any_binding_bytes':8,'qml_property_bytes':8,'business_script_evaluation':False}
