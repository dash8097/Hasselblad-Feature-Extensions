#pragma once
#include "multishot_settings.hpp"
#include "stock_factory.hpp"
#include "signal_bridge.hpp"
namespace x2dui {
// Caller reads these metrics from the existing factory Constants singleton.
struct SettingsStyle {
 double headerHeight=0,rowHeight=0,leftMargin=0,rightMargin=0,labelFont=0,valueFont=0,headerFont=0;
 double selectorLeft=0,selectorRight=0,selectorTop=0,selectorBottom=0,visibleChoices=0;
 unsigned background=0,foreground=0xffffff,highlight=0xde4200,border=0x646464;
};
enum class SettingsKey {Back,External,Up,Down,Accept};
class MultishotSettingsView final:private LocalHeap {
 struct Binding:LocalHeap {MultishotSettingsView*self;int index;Binding(MultishotSettingsView*s,int i):self(s),index(i){}};
 ViewApi api;StockProfile stock;AbiViewProfile*abi;
 MultishotSettings model;SettingsChoices options;SettingsStyle style;
 void*root=nullptr,*header=nullptr,*title=nullptr,*clip=nullptr,*content=nullptr,*popup=nullptr,*shade=nullptr,*outside=nullptr,*frame=nullptr,*panel=nullptr,*highlight=nullptr;
 void*rows[10]{},*rowHighlight[10]{},*touch[10]{},*labels[7]{},*buttons[7]{};SignalWatch*backWatch=nullptr;
 int refs=1,selected=0,pressIndex=0;double width=0,height=0,offset=0,pressY=0,pressOffset=0;bool alive=true,closed=false,dragging=false,moved=false,interactive=true;
 bool(*changed)(void*,const SettingsChange&)=nullptr;void(*left)(void*,SettingsExit)=nullptr;void*context=nullptr;
 MultishotSettingsView(ViewApi a,StockProfile s,SignalProfile,AbiViewProfile*b):api(a),stock(s),abi(b){}
 void retain(){++refs;}void drop(){if(!--refs)delete this;}
 static void event(void*,Event,double);static void released(void*);static void back(void*);static void releaseBack(void*);
 bool bind(void*,Event,int);bool redraw();void arrange();void openRow(int);void accept();void hidePopup();
public:
 static MultishotSettingsView*create(ViewApi,StockProfile,SignalProfile,AbiViewProfile*,void*contextObject,void*parent,bool(*changed)(void*,const SettingsChange&),void(*left)(void*,SettingsExit),void*);
 bool update(const SettingsSnapshot&,const SettingsStyle&,double width,double height,bool interactive=true,bool external=false);
 void key(SettingsKey);void close();void*item()const{return root;}
 bool popup_open()const{return model.popup;}int selection()const{return selected;}int row()const{return model.currentRow;}
};
}
