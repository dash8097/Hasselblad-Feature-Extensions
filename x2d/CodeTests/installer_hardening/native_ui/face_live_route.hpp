#pragma once
#include "stock_factory.hpp"
#include "menu_discovery.hpp"
namespace x2dui {
struct FaceLiveRoute {void*scene=nullptr,*indicator=nullptr,*viewModel=nullptr,*face=nullptr,*touchpad=nullptr,*overlay=nullptr;};
// Read-only current visual route, checked immediately before a user action.
// Presence in a stale QObject tree or an old property is not enough.
bool face_live_route_current(const StockProfile&,const MenuProfile&,const FaceLiveRoute&);
}
