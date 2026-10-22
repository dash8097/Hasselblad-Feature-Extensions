#pragma once
#include "pixel_host_state.hpp"
#include "stock_factory.hpp"
namespace x2dui {
struct AlbumProfile {
 void*const*slot=nullptr;const void*meta=nullptr;
 const void*(*cast)(const void*,const void*)=nullptr;
 void(*path_changed)(void*,const AbiString&)=nullptr;
 void(*enter_dir)(void*,const AbiString&)=nullptr;
 void(*update_loading)(void*)=nullptr;
};
class AlbumAdapter {
 AlbumProfile profile;StockProfile stock;MenuProfile menu;
 void*object()const;
public:
 AlbumAdapter(AlbumProfile p,StockProfile s,MenuProfile m):profile(p),stock(s),menu(m){}
 bool snapshot(void*freshRoot,bool storageBrowsing,AlbumContext&)const;
 // Only the three original album actions. Refuses refresh while a photograph
 // is being viewed or either original browsing flag is active.
 bool dispatch(const PixelHostAction&,void*freshRoot,bool storageBrowsing)const;
};
}
