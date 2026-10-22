#pragma once
#include <cstdint>
#include "local_heap.hpp"
// Scalar bridge: implementations use an existing Qt runtime. No QML/JS.
namespace x2dui {
enum class Kind {Item,Rectangle,Text,Mouse};
enum class Event {Click,Press,Move,Release,Destroy,Resize,Hidden};
enum class EffectKey {Up,Down,Accept,Abort};
using Handler=void(*)(void*,Event,double);
struct ViewApi {
 void*context;
 void*(*create)(void*,Kind,void*);
 void(*destroy)(void*,void*);
 void(*rect)(void*,void*,double,double,double,double);
 void(*color)(void*,void*,unsigned,bool);
 void(*text)(void*,void*,const char16_t*,double,int);
 void(*visible)(void*,void*,bool);
 void(*enabled)(void*,void*,bool);
 void(*opacity)(void*,void*,double);
 void(*name)(void*,void*,const char*);
 void(*layer)(void*,void*,double,bool);
 bool(*listen)(void*,void*,Event,Handler,void*,void(*)(void*));
};
class EffectView final:private LocalHeap {
 struct Binding:LocalHeap {EffectView*self;int index;Binding(EffectView*s,int i):self(s),index(i){}};
 ViewApi api;void*root=nullptr,*popup=nullptr,*panel=nullptr,*frame=nullptr,*highlight=nullptr,*value=nullptr,*entry=nullptr,*background=nullptr,*title=nullptr;
 void*labels[16]{},*buttons[16]{};void*shade=nullptr,*dismiss=nullptr;
 const char16_t*const*options=nullptr;unsigned optionCount=3;
 int refs=1,mode=0,selected=0,paintedMode=-2;bool confirmed=false;
 double labelSize=51.2,valueSize=51,leftMargin=51.2,rightMargin=51.2,textTop=25.6;const char16_t*captionText=nullptr;bool alive=true,available=false,open=false,dragging=false,moved=false;
 double width=800,height=600,originY=0,rowHeight=88,pressY=0;int pressIndex=0;
 void(*choose)(void*,int)=nullptr;void*chooseContext=nullptr;
 static void event(void*,Event,double);static void released(void*);
 bool bind(void*,Event,int);void setOpen(bool);void arrange();void select(int);
 void retain(){++refs;}void drop(){if(--refs==0)delete this;}
 explicit EffectView(ViewApi a):api(a){}
 ~EffectView()=default;
public:
 static EffectView*create(ViewApi,void*host,void*overlay,void(*choose)(void*,int),void*context);
 static EffectView*createChoices(ViewApi,void*,void*,void(*)(void*,int),void*,const char16_t*,const char16_t*const*,unsigned,const char*);
 void valueText(const char16_t*t){if(alive&&value)api.text(api.context,value,t,28,2);}
 void update(int,bool,bool pending=false);void style(double,double,double,double,double);void place(double,double);void resize(double,double);void close();void*item()const{return root;}
 bool popupOpen()const{return open;}int selection()const{return selected;}
 bool key(EffectKey);
};
}
