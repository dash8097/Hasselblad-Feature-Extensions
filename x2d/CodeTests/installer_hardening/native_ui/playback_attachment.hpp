#pragma once
#include "pixel_viewport_view.hpp"
#include "tree_observer.hpp"
namespace x2dui {
// Finds and borrows the sole original TiledImage below FlickablePreview.
// This layer only corrects tile readiness; it does not yet alter zoom-step
// policy or source debounce, so its success is not full p48 readiness.
class PlaybackAttachment final {
 StockProfile stock;MenuProfile menu;BindingProfile binding;SignalProfile signals;
 PixelViewportView viewport;TreeObserver tree;void*scene=nullptr,*candidate=nullptr;SignalWatch*candidateDeath=nullptr;
 static void died(void*p){static_cast<PlaybackAttachment*>(p)->candidate=nullptr;}static void released(void*){}
 void clearCandidate(){signal_unwatch(candidateDeath);candidateDeath=nullptr;candidate=nullptr;}
 bool locate(void*,void*&,unsigned&,unsigned&,unsigned depth=0)const;
 unsigned tileTexture(void*,unsigned&,unsigned depth=0)const;
public:
 PlaybackAttachment(StockProfile s,MenuProfile m,AbiViewProfile*a,BindingProfile b,SignalProfile g):stock(s),menu(m),binding(b),signals(g),viewport(s,m,a),tree(m,abi_view_api(a),g){}
 bool tick(void*freshRoot,uint64_t now);
 ~PlaybackAttachment(){close();}
 void close(){viewport.close();clearCandidate();tree.reset();scene=nullptr;}
 void*item()const{return viewport.item();}
 const PixelViewportCore&state()const{return viewport.state();}
 void*active_image()const{return viewport.active_image();}
};
}
