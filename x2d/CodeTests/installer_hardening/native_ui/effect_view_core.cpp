#include "effect_view_core.hpp"
#include <new>
#include <cmath>
namespace x2dui {
static const char16_t*const names[]={u"关",u"动画",u"动画与声音"};
bool EffectView::bind(void*object,Event e,int index){
 auto*b=new(std::nothrow)Binding{this,index};if(!b)return false;retain();
 if(api.listen(api.context,object,e,event,b,released))return true;
 released(b);return false;
}
void EffectView::released(void*opaque){auto*b=static_cast<Binding*>(opaque);auto*self=b->self;delete b;self->drop();}
void EffectView::event(void*opaque,Event e,double y){
 auto*b=static_cast<Binding*>(opaque);auto&s=*b->self;
 if(e==Event::Destroy){s.alive=false;s.root=nullptr;s.setOpen(false);if(s.popup){auto*p=s.popup;s.popup=nullptr;s.api.destroy(s.api.context,p);}return;}
 if(!s.alive)return;
 if(e==Event::Hidden){s.setOpen(false);return;}
 if(e==Event::Click){if(b->index==-1){if(s.available){s.selected=s.mode;s.setOpen(true);}}else if(b->index==-2)s.setOpen(false);else s.select(b->index);return;}
 if(!s.open||!s.available)return;
 if(e==Event::Press){s.dragging=true;s.moved=false;s.pressY=y;s.pressIndex=s.selected;}
 else if(e==Event::Move&&s.dragging){double delta=s.pressY-y;if(delta>8||delta< -8)s.moved=true;double cell=s.height*(316.0/480.0)/4.3;int change=static_cast<int>(delta/cell+(delta>=0?.5:-.5));int next=s.pressIndex+change;if(next<0)next=0;if(next>=int(s.optionCount))next=int(s.optionCount)-1;s.selected=next;s.arrange();}
 else if(e==Event::Release){bool wasDown=s.dragging;s.dragging=false;if(wasDown&&!s.moved)s.select(b->index);}
}
void EffectView::setOpen(bool next){open=next;dragging=false;if(popup)api.visible(api.context,popup,next);if(next)arrange();}
void EffectView::select(int index){if(!available||index<0||index>=int(optionCount))return;setOpen(false);if(choose)choose(chooseContext,index);}
bool EffectView::key(EffectKey key){if(!alive||!open)return false;if(key==EffectKey::Abort){setOpen(false);return true;}if(!available)return false;if(key==EffectKey::Accept){select(selected);return true;}if(key==EffectKey::Up&&selected>0)--selected;else if(key==EffectKey::Down&&selected+1<int(optionCount))++selected;arrange();return true;}
void EffectView::arrange(){
 auto c=api.context;if(!root)return;api.rect(c,root,0,originY,width,rowHeight);api.rect(c,background,0,0,width,rowHeight);api.rect(c,title,leftMargin,textTop,width*.58-leftMargin,labelSize);api.rect(c,value,width*.58,textTop,width*.42-rightMargin,labelSize);api.rect(c,entry,0,0,width,rowHeight);
 if(!popup)return;api.rect(c,popup,0,0,width,height);api.rect(c,shade,0,0,width,height);api.rect(c,dismiss,0,0,width,height);
 double left=width*260/640,right=width*24/640,top=height*82/480,pw=width-left-right,ph=height*316/480,cell=ph/4.3;
 api.rect(c,frame,left,top,pw,ph);api.rect(c,panel,2,2,pw-4,ph-4);api.rect(c,highlight,0,(ph-cell)/2-2,pw-4,cell);
 for(int i=0;i<int(optionCount);i++){double y=(ph-cell)/2+(i-selected)*cell;api.rect(c,labels[i],2,y,pw-4,cell);api.rect(c,buttons[i],2,y,pw-4,cell);api.color(c,labels[i],i==selected?0xffffff:0xd3d3d3,true);}
}
EffectView*EffectView::create(ViewApi a,void*host,void*overlay,void(*choose)(void*,int),void*context){
 return createChoices(a,host,overlay,choose,context,u"快门动画与声音",names,3,"X2dExposureEffectOption");
}
EffectView*EffectView::createChoices(ViewApi a,void*host,void*overlay,void(*choose)(void*,int),void*context,const char16_t*caption,const char16_t*const*choices,unsigned count,const char*identifier){
 if(!caption||!choices||!identifier||!count||count>16)return nullptr;
 for(unsigned i=0;i<count;i++)if(!choices[i])return nullptr;
 if(!host||!overlay||!a.create||!a.destroy||!a.rect||!a.color||!a.text||!a.visible||!a.enabled||!a.opacity||!a.name||!a.layer||!a.listen)return nullptr;
 auto*s=new(std::nothrow)EffectView(a);if(!s)return nullptr;s->captionText=caption;s->choose=choose;s->chooseContext=context;s->options=choices;s->optionCount=count;
 auto make=[&](Kind k,void*p){return a.create(a.context,k,p);};
 s->root=make(Kind::Item,host);if(!s->root){delete s;return nullptr;}
 if(!s->bind(s->root,Event::Destroy,-3)){a.destroy(a.context,s->root);delete s;return nullptr;}
 auto fail=[&](){s->close();return static_cast<EffectView*>(nullptr);};
 a.name(a.context,s->root,identifier);
 auto*bg=make(Kind::Rectangle,s->root);s->background=bg;auto*title=make(Kind::Text,s->root);s->title=title;s->value=make(Kind::Text,s->root);s->entry=make(Kind::Mouse,s->root);
 s->popup=make(Kind::Item,overlay);if(!bg||!title||!s->value||!s->entry||!s->popup)return fail();
 a.name(a.context,s->popup,"X2dNativeEffectPopup");a.layer(a.context,s->popup,2000,false);a.rect(a.context,bg,0,0,800,88);a.color(a.context,bg,0,false);a.rect(a.context,title,30,0,410,88);a.text(a.context,title,caption,s->labelSize,false);a.color(a.context,title,0xffffff,true);
 s->shade=make(Kind::Rectangle,s->popup);s->dismiss=make(Kind::Mouse,s->popup);s->frame=make(Kind::Rectangle,s->popup);
 if(!s->shade||!s->dismiss||!s->frame)return fail();s->panel=make(Kind::Rectangle,s->frame);if(!s->panel)return fail();a.color(a.context,s->shade,0,false);a.opacity(a.context,s->shade,.7);a.color(a.context,s->frame,0x646464,false);a.color(a.context,s->panel,0,false);a.layer(a.context,s->panel,0,true);
 s->highlight=make(Kind::Rectangle,s->panel);if(!s->highlight)return fail();a.color(a.context,s->highlight,0xde4200,false);
 if(!s->bind(s->entry,Event::Click,-1)||!s->bind(s->dismiss,Event::Click,-2)||!s->bind(s->root,Event::Hidden,-3))return fail();
 for(int i=0;i<int(count);i++){s->labels[i]=make(Kind::Text,s->panel);s->buttons[i]=make(Kind::Mouse,s->panel);if(!s->labels[i]||!s->buttons[i])return fail();a.text(a.context,s->labels[i],choices[i],s->valueSize,true);
  if(!s->bind(s->buttons[i],Event::Press,i)||!s->bind(s->buttons[i],Event::Move,i)||!s->bind(s->buttons[i],Event::Release,i))return fail();}
 s->setOpen(false);s->update(0,false);s->arrange();return s;
}
void EffectView::update(int next,bool ready,bool pending){
 if(!alive)return;
 if(next<0||next>=int(optionCount))ready=false;else if(ready){mode=next;confirmed=true;}
 available=ready;int display=(ready||(pending&&confirmed))?mode:-1;
 if(display!=paintedMode){api.text(api.context,value,display>=0?options[display]:u"暂不可用",valueSize,2);api.color(api.context,value,0xffffff,true);paintedMode=display;}
 api.enabled(api.context,entry,available);for(auto*b:buttons)if(b)api.enabled(api.context,b,available);if(!available)setOpen(false);
}
void EffectView::style(double label,double val,double left,double right,double top){
 if(!(label>0&&val>0&&left>=0&&right>=0&&top>=0))return;
 if(label==labelSize&&val==valueSize&&left==leftMargin&&right==rightMargin&&top==textTop)return;
 labelSize=label;valueSize=val;leftMargin=left;rightMargin=right;textTop=top;
 if(alive){for(unsigned i=0;i<optionCount;i++)if(labels[i])api.text(api.context,labels[i],options[i],valueSize,1);api.text(api.context,title,captionText,labelSize,0);int display=paintedMode;paintedMode=-2;if(display>=-1){api.text(api.context,value,display>=0?options[display]:u"暂不可用",valueSize,2);paintedMode=display;}arrange();}
}
void EffectView::place(double y,double h){if(std::isfinite(y)&&std::fabs(y)<10000000&&h>=32&&h<=256){originY=y;rowHeight=h;arrange();}}
void EffectView::resize(double w,double h){if(w>0&&h>0){width=w;height=h;arrange();}}
void EffectView::close(){if(root){auto*p=root;root=nullptr;alive=false;api.destroy(api.context,p);}else if(popup){auto*p=popup;popup=nullptr;api.destroy(api.context,p);}drop();}
}
