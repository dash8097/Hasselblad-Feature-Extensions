#pragma once
#include "screen_controls.hpp"
#include "stock_factory.hpp"
namespace x2dui {
struct ScreenControlStyle {
 double rowHeight=0,left=0,right=0,labelFont=0,valueFont=0,descriptionFont=0,descriptionHeight=0;
 double selectorLeft=0,selectorRight=0,selectorTop=0,selectorBottom=0,visibleChoices=0;
 unsigned background=0,foreground=0xffffff,highlight=0xde4200,border=0x646464;
 const char16_t*title=nullptr;bool showSwitch=true,highlighted=false;int fontWeight=500;const char16_t*description=nullptr;
};
enum class ScreenControlKey {Open,Up,Down,Accept,Abort,HalfPress,Navigation};
class ScreenControlView final:private LocalHeap {
 struct Binding:LocalHeap {ScreenControlView*self;int index;Binding(ScreenControlView*s,int i):self(s),index(i){}};
 ViewApi api_;StockProfile stock_;AbiViewProfile*abi_;ScreenControl*model_;ScreenControlStyle style_;
 void*root_=nullptr,*row_=nullptr,*description_=nullptr,*entry_=nullptr,*popup_=nullptr,*shade_=nullptr,*outside_=nullptr,*frame_=nullptr,*panel_=nullptr,*highlight_=nullptr;
 void*labels_[7]{},*buttons_[7]{};double width_=0,overlayWidth_=0,height_=0,y_=0,pressY_=0;
 double position_=0,pressPosition_=0;uint64_t animationTime_=0;
 int selected_=-1,pressIndex_=0,refs_=1,choiceIndex_[7]{};
 bool alive_=true,closed_=false,open_=false,interactive_=true,dragging_=false,moved_=false;
 bool(*choose_)(void*,int)=nullptr;void*context_=nullptr;
 ScreenControlView(ViewApi a,StockProfile s,AbiViewProfile*b,ScreenControl*m):api_(a),stock_(s),abi_(b),model_(m){}
 void retain(){++refs_;}void drop(){if(!--refs_)delete this;}
 static void event(void*,Event,double);static void released(void*);void handle(int,Event,double);bool bind(void*,Event,int);
 void arrange();void hide();void show();void accept();
public:
 static ScreenControlView*create(ViewApi,StockProfile,AbiViewProfile*,ScreenControl*,void*contextObject,void*host,void*overlay,bool(*choose)(void*,int),void*);
 bool update(const ScreenControlStyle&,double width,double height,double rowY,bool touchEnabled,double overlayWidth=0);
 bool key(ScreenControlKey);void close();void*item()const{return root_;}
 void advance(uint64_t now);
 void*description_item()const{return description_;}
 bool popup_open()const{return open_;}int selection()const{return selected_;}
};
}
