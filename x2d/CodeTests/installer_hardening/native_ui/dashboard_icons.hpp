#pragma once
#include "binding_lease.hpp"
#include "menu_discovery.hpp"
#include <cmath>
namespace x2dui {
// GUI-only adaptation. The caller supplies objects from its existing structural
// discovery; no timer, recursive search, proxy write or popup state is owned here.
class DashboardIcons {
 StockProfile stock;MenuProfile menu;BindingProfile binding;SignalProfile signals;
 void*af=nullptr,*drive=nullptr;BindingLease afSymbol,driveSymbol,driveScale;
 int lastFocus=-1,lastExtension=-1;bool blockedFocus=false,blockedDrive=false;double scale=1;
 bool number(void*o,const char*n,double&v){if(!o)return false;auto x=menu.property(o,n);bool ok=false;v=menu.number(&x,&ok);menu.variant_destroy(&x);return ok&&std::isfinite(v);}
public:
 DashboardIcons(StockProfile s,MenuProfile m,BindingProfile b,SignalProfile g):stock(s),menu(m),binding(b),signals(g){}
 void close(){afSymbol.close();driveSymbol.close();driveScale.close();af=drive=nullptr;lastFocus=lastExtension=-1;blockedFocus=blockedDrive=false;}
 void targets(void*a,void*d){if(a!=af){afSymbol.close();af=a;lastFocus=-1;blockedFocus=false;}if(d!=drive){driveSymbol.close();driveScale.close();drive=d;lastExtension=-1;blockedDrive=false;}}
 bool update(void*model,int extension){
  bool ok=true;double mode=0,selectable=0;
  if(af&&!blockedFocus){
   if(!number(model,"focusMode",mode)||!number(model,"focusModeSelectable",selectable)||mode<1||mode>3||std::floor(mode)!=mode||(selectable!=0&&selectable!=1)){afSymbol.close();lastFocus=-1;}
   else {int key=int(mode)+(selectable?0:3);if(key!=lastFocus){
    static const char16_t*icons[]={u"image://svg/ic_controlscreen_focus_mode_AF-S",u"image://svg/ic_controlscreen_focus_mode_AF-C",u"image://svg/ic_controlscreen_focus_mode_MF",u"image://svg/ic_controlscreen_focus_mode_AF-S_disabled",u"image://svg/ic_controlscreen_focus_mode_AF-C_disabled",u"image://svg/ic_controlscreen_focus_mode_MF_disabled"};
    if((!afSymbol.alive()&&!afSymbol.acquire(stock,binding,signals,af,u"symbol"))||!afSymbol.text(icons[key-1])){afSymbol.close();blockedFocus=true;ok=false;}else lastFocus=key;
   }}
  }
  if(drive&&!blockedDrive&&extension!=lastExtension){
   if(extension!=1&&extension!=2){driveSymbol.close();driveScale.close();lastExtension=0;}
   else {
    bool acquired=driveSymbol.alive();
    if(!acquired)acquired=number(drive,"symbolScale",scale)&&driveSymbol.acquire(stock,binding,signals,drive,u"symbol")&&driveScale.acquire(stock,binding,signals,drive,u"symbolScale");
    if(!acquired||!driveSymbol.text(extension==2?u"file:///system/etc/x2d-pixel-overclock/stack.svg":u"file:///system/etc/x2d-pixel-overclock/multishot.svg")||!driveScale.number(scale*44/64)){driveSymbol.close();driveScale.close();blockedDrive=true;ok=false;}else lastExtension=extension;
   }
  }
  return ok;
 }
};
}
