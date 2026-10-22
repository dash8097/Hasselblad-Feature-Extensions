#pragma once
#include "drive_popup.hpp"
#include "stock_factory.hpp"
#include "signal_bridge.hpp"
namespace x2dui {
struct DriveSummary {int count=0;UiText labels[10],values[10],caption;};
struct DriveStyle {double scale=0,scaleX=0,scaleY=0;unsigned background=0,foreground=0xffffff,border=0x646464;};
enum class DriveKey {Up,Down,Left,Right,Accept,Parameters,Close};
class DrivePopupView final:private LocalHeap {
 struct Binding:LocalHeap {DrivePopupView*self;int index;Binding(DrivePopupView*s,int i):self(s),index(i){}};
 ViewApi api;StockProfile stock;DrivePopup model;DriveStyle style;
 void*root=nullptr,*shade=nullptr,*outside=nullptr,*frame=nullptr,*panel=nullptr,*divider=nullptr,*heading=nullptr,*summaryHeading=nullptr,*summaryTouch=nullptr,*caption=nullptr;
 void*icons[8]{},*rows[10]{};SignalWatch*presses[8]{};int refs=1;bool closed=false,alive=true,interactive=true,configured=false;
 bool rendered=false;int renderedIndex=-1;DriveSummary renderedSummary;
 DrivePermissions priorPermissions{};DriveChoice priorActual=DriveChoice::Single;ExtensionMode priorExtension=ExtensionMode::None;double width=0,height=0;
 // False means rejected before accepting any intent; it is not a hardware acknowledgement.
 bool(*dispatch)(void*,const DriveAction*,unsigned)=nullptr;bool(*summary)(void*,DriveChoice,DriveSummary&)=nullptr;void*context=nullptr;
 DrivePopupView(ViewApi a,StockProfile s,SignalProfile):api(a),stock(s){}
 void retain(){++refs;}void drop(){if(!--refs)delete this;}
 static void event(void*,Event,double);static void pressed(void*);static void release(void*);
 bool bind(void*,Event,int);bool flush();bool redraw();
public:
 static DrivePopupView*create(ViewApi,StockProfile,SignalProfile,void*contextObject,void*parent,bool(*dispatch)(void*,const DriveAction*,unsigned),bool(*summary)(void*,DriveChoice,DriveSummary&),void*);
 bool update(DrivePermissions,DriveChoice actual,ExtensionMode selected,const DriveStyle&,double,double,bool touchEnabled=true);
 bool reopen_display();
 void key(DriveKey);void close();void*item()const{return root;}
};
}
