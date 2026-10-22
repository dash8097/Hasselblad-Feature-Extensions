#pragma once
#include "pixel_io_pump.hpp"
#include "pixel_host_state.hpp"
#include "processing_host.hpp"
namespace x2dui {
// GUI-thread coordinator. The caller owns the worker transport and supplies a
// fresh original root + current album snapshot; action dispatch never invents
// successful original-proxy completion. No capture request endpoint exists.
class PixelGuiRuntime {
 StockProfile stock;MenuProfile menu;ViewApi api;
 PixelController controller_;PixelHost host_;PixelIoPump pump;ProcessingHost page;
 void*bubble=nullptr,*bubbleText=nullptr,*bubbleScene=nullptr;
 static void pageAction(void*,ProcessingAction);static void bubbleEvent(void*,Event,double);static void release(void*){}
 bool showPreparation(void*,const UiText&,double,double,double,double);void closeBubble();
public:
 PixelGuiRuntime(StockProfile,MenuProfile,AbiViewProfile*,SignalProfile,KeyProfile,KeyMapProfile,FocusProfile,PixelIoApi,PixelFeatures={});
 ~PixelGuiRuntime();
 PixelController&controller(){return controller_;}PixelHost&host(){return host_;}
 bool tick(void*freshRoot,uint64_t now,const AlbumContext&,double width,double height,double scale=1,double scaleX=1,double scaleY=1);
 // Must drain and dispatch every action in order, using original typed adapters.
 void*processing_item()const{return page.item();}
 bool take_action(PixelHostAction&a){return host_.take_action(a);}
 void close(void*freshRoot);
};
}
