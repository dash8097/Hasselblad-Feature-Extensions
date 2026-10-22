#pragma once
#include "face_popup_view.hpp"
#include "focus_model.hpp"
namespace x2dui {
struct FocusPopupState {FocusSnapshot source;int selected=-1,pending=-1;bool failed=false,closed=false;};
struct FocusPopupStyle {double scaleX=1,scaleY=1,scale=1,headerHeight=64;unsigned foreground=0xffffff;const char16_t*heading=u"Focus Mode";};
class FocusPopupView final:private LocalHeap {
 struct Binding:LocalHeap {FocusPopupView*self;int index;Binding(FocusPopupView*s,int i):self(s),index(i){}};
 struct Scope {FocusPopupView*v;explicit Scope(FocusPopupView*p):v(p){v->retain();}~Scope(){v->drop();}};
 ViewApi api_;StockProfile stock_;SignalProfile signals_;void*root_=nullptr,*content_=nullptr,*status_=nullptr,*choice_[3]{},*icons_[3]{},*outside_[4]{};SignalWatch*watches_[4]{};
 FocusPopupState state_;FocusPopupStyle style_;char16_t heading_[256]{};double width_=0,height_=0;int selected_=-1,refs_=1;
 bool broken_=false,closed_=false,finished_=false,touch_=true,latched_=false,sawPending_=false,dispatching_=false;
 bool(*choose_)(void*,int)=nullptr;void(*abort_)(void*)=nullptr;void*context_=nullptr;
 FocusPopupView(ViewApi a,StockProfile s,SignalProfile g):api_(a),stock_(s),signals_(g){}
 void retain(){++refs_;}void drop(){if(!--refs_)delete this;}bool live()const{return root_&&!closed_&&!broken_;}
 static void event(void*,Event,double);static void signal(void*);static void released(void*);
 bool bind(void*,Event,int);bool watch(void*,const char*,int,int);void handle(int);bool selectable(int)const;bool refresh();void accept();void abort();
public:
 static FocusPopupView*create(ViewApi,StockProfile,SignalProfile,FacePopupProfile,void*contextObject,void*parent,bool(*choose)(void*,int),void(*abort)(void*),void*);
 bool update(const FocusPopupState&,const FocusPopupStyle&,double width,double height,bool touchEnabled);
 bool key(FocusKey);void applied();void close();void*item()const{return root_;}bool finished()const{return finished_;}int selection()const{return selected_;}
};
}
