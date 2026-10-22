#pragma once
#include "protocol_text.hpp"
namespace x2dui {
// 仅更新关于页指定原厂右侧值标签，不修改底层机型或 ValueFormatter。
UiText about_display_text(const char*itemName,bool aboutPage,bool rightAnchored,const UiText&oldLabel,const UiText&formatted);
}
