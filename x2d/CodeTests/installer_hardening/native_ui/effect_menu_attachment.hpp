#pragma once
#include "menu_discovery.hpp"
#include "esflash_client.h"
#include "esflash_timing.hpp"
#include "binding_lease.hpp"
#include "screen_key_profile.hpp"
#include "key_mapping.hpp"
namespace x2dui {
// Caller supplies a freshly enumerated, live GUI root on the GUI thread.
// No saved foreign QObject is dereferenced before walking that current tree.
class EffectMenuAttachment {
 MenuProfile menu;ViewApi api;EffectView*view=nullptr;void*list=nullptr;
 double fontLabel=51.2,fontValue=51,leftMargin=51.2,rightMargin=51.2,textTop=25.6;
 double margin=0;void(*choose)(void*,int);void*context;
 FlashTiming flashTiming;
 StockProfile stock;BindingProfile binding;SignalProfile signals;BindingLease scroll;
 void*page=nullptr;SignalWatch*pageDeath=nullptr;
 static void page_destroyed(void*p){static_cast<EffectMenuAttachment*>(p)->page=nullptr;}
 static void released(void*){}
 void*blockedList=nullptr;
 ScreenKeyProfile keys_{};KeyMapProfile keyMap_{};ScreenKeyLease keyLease_{{},{}};
 ScreenKeyReceiver*keyReceiver_=nullptr;void*keyItem_=nullptr;void*scene_=nullptr;int heldKey_=0;
 SignalWatch*keyDeath_=nullptr;static void key_died(void*p){static_cast<EffectMenuAttachment*>(p)->keyItem_=nullptr;}
 static bool physical(void*,const KeyInput&,bool);
 void update_keys();void clear_keys();
 bool number(void*,const char*,double&);
 bool update_scroll(const MenuTarget&,double);
 void detach(void*root);
public:
 EffectMenuAttachment(MenuProfile m,ViewApi a,void(*c)(void*,int),void*ctx,StockProfile s,BindingProfile b,SignalProfile g):menu(m),api(a),choose(c),context(ctx),stock(s),binding(b),signals(g){}
 ~EffectMenuAttachment(){clear_keys();signal_unwatch(pageDeath);if(view)view->close();} // Explicit close(root) restores owned margin.
 EffectMenuAttachment(const EffectMenuAttachment&)=delete;
 int tick(void*root,int mode,bool ready,double width,double height,bool pending=false);
 void flashService(X2dFlashClient*c,uint64_t now,const FlashExposure&e={}){if(c)flashTiming.tick(*c,e,now);}
 void close(void*liveRoot){detach(liveRoot);}
 void*item()const{return view?view->item():nullptr;}
 bool belongs(void*root)const{return view&&view->item()&&menu_contains(menu,root,list);}
 void refresh(int mode,bool ready,bool pending=false);
 bool configure_keys(ScreenKeyProfile p,KeyMapProfile map){void*scene=scene_;clear_keys();scene_=scene;keys_=p;keyMap_=map;return keyLease_.configure(p,signals);}
 void style(double label,double value,double left,double right,double top){fontLabel=label;fontValue=value;leftMargin=left;rightMargin=right;textTop=top;if(view)view->style(label,value,left,right,top);}
};
}
