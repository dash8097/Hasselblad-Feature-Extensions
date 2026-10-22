#pragma once
#include "menu_discovery.hpp"
#include "protocol_text.hpp"
namespace x2dui {
struct TranslationProfile {
 AbiArray(*translate)(const char*,const char*,const char*,int)=nullptr;
 void(*deallocate)(void*,std::ptrdiff_t,std::ptrdiff_t)=nullptr;
};
// Fixed original translation key; no caller-selected source or context.
UiText factory_screen_notice(const TranslationProfile&);
UiText factory_liveview_title(unsigned language);
UiText factory_liveview_choice(const TranslationProfile&,unsigned index);
}
