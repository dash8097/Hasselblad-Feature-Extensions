#pragma once
#include "pixel_viewport_core.hpp"
#include "processing_host.hpp"
#include "binding_lease.hpp"
namespace x2dui {
// Uses the original in-process TiledImage resource, whose shader retains OES,
// LUT and hardware compensation. No copied QML and no source-path log handlers.
class PixelViewportView final {
 StockProfile stock;MenuProfile menu;ViewApi api;void*root=nullptr;unsigned texture_=0;PixelViewportCore core_;bool borrowed_=false;BindingLease statusLease;
 static void event(void*,Event,double);static void release(void*){}
 bool scan(void*,unsigned&,bool*,void*&,uint64_t,bool,unsigned depth=0)const;
public:
 PixelViewportView(StockProfile s,MenuProfile m,AbiViewProfile*a):stock(s),menu(m),api(abi_view_api(a)){}
 ~PixelViewportView(){close();}
 PixelViewportView(const PixelViewportView&)=delete;PixelViewportView&operator=(const PixelViewportView&)=delete;
 bool attach(void*parent);
 // Borrow original TiledImage: no component creation or source/geometry writes.
 bool borrow(void*original,BindingProfile,SignalProfile,unsigned maxTexture);
 bool observe_existing(ProtocolBytes,unsigned width,unsigned height,uint64_t now);
 bool set(ProtocolBytes,unsigned width,unsigned height,unsigned maxTexture,uint64_t now);
 bool tick(uint64_t now,bool overExposureRunning);
 void close();void*item()const{return borrowed_&&!statusLease.alive()?nullptr:root;}const PixelViewportCore&state()const{return core_;}
 void*active_image()const;
};
}
