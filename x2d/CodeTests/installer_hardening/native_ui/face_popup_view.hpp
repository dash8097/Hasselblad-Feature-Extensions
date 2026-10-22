#pragma once
#include "stock_factory.hpp"
#include "signal_bridge.hpp"
#include "face_model.hpp"
namespace x2dui {
// Actual QVariant carries QQmlListProperty<QQuickItem>, verified by fixed metatype.
struct FacePopupList {void*object,*data;void(*append)(FacePopupList*,void*);std::ptrdiff_t(*count)(FacePopupList*);void*(*at)(FacePopupList*,std::ptrdiff_t);void(*clear)(FacePopupList*);void(*replace)(FacePopupList*,std::ptrdiff_t,void*);void(*removeLast)(FacePopupList*);};
struct FacePopupProfile {const void*listType=nullptr;};
struct FacePopupSnapshot {bool attached=false,canChange=false,busy=false,error=false,multiAllowed=false,offAllowed=false;int current=-1,rawMode=0;};
struct FacePopupStyle {double scaleX=1,scaleY=1,scale=1,headerHeight=64;unsigned foreground=0xffffff;const char16_t*heading=u"人脸检测";};
class FacePopupView final:private LocalHeap {
 struct Binding:LocalHeap {FacePopupView*self;int index;Binding(FacePopupView*s,int i):self(s),index(i){}};
 ViewApi api_;StockProfile stock_;SignalProfile signals_;FacePopupProfile list_;
 void*root_=nullptr,*content_=nullptr,*status_=nullptr,*choice_[3]{},*outside_[4]{};SignalWatch*watches_[4]{};
 FacePopupSnapshot snapshot_;FacePopupStyle style_;char16_t heading_[256]{};double width_=0,height_=0;int selected_=-1,refs_=1;
 bool closed_=false,finished_=false,touch_=true;bool(*choose_)(void*,int)=nullptr;void*context_=nullptr;
 FacePopupView(ViewApi a,StockProfile s,SignalProfile g,FacePopupProfile l):api_(a),stock_(s),signals_(g),list_(l){}
 void retain(){++refs_;}void drop(){if(!--refs_)delete this;}
 static void event(void*,Event,double);static void signal(void*);static void released(void*);
 bool bind(void*,Event,int);bool watch(void*,const char*,int,int);void handle(int);bool selectable(int)const;bool refresh();void accept();
public:
 static FacePopupView*create(ViewApi,StockProfile,SignalProfile,FacePopupProfile,void*contextObject,void*parent,bool(*choose)(void*,int),void*);
 bool update(const FacePopupSnapshot&,const FacePopupStyle&,double width,double height,bool touchEnabled);
 bool key(FacePopupKey);void applied();void close();void*item()const{return root_;}
 bool finished()const{return finished_;}int selection()const{return selected_;}
};
}
