#pragma once
#include "menu_discovery.hpp"
#include "protocol_text.hpp"
namespace x2dui {
struct FactoryFormatterProfile {void*const*slot=nullptr;const void*meta=nullptr;const void*(*cast)(const void*,const void*)=nullptr;AbiMetaCall call=nullptr;void(*string_ctor)(void*,const char16_t*,std::ptrdiff_t)=nullptr;void(*string_deallocate)(void*,std::ptrdiff_t,std::ptrdiff_t)=nullptr;};
// Whitelisted read-only methods: getDisplayValue(6), getDisplayAltValue(7).
// Never calls instance() or setSettingValue(5).
bool factory_format(const FactoryFormatterProfile&,const MenuProfile&,const UiText&name,const AbiVariant&value,bool alternate,UiText&out);
}
