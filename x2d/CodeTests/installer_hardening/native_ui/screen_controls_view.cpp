#include "screen_controls_view.hpp"
#include <new>
#include <cmath>
#include <cstdio>
namespace x2dui {namespace {
bool number(double v){return std::isfinite(v)&&v>=0&&v<=16384;}
void text_size(AbiViewProfile*a,void*o,double pixels){alignas(8)unsigned char font[16];a->font_ctor(font);int status=-1,flags=0;void*args[]={font,nullptr,&status,&flags};a->text(o,1,1,args);a->font_size(font,int(pixels));a->text(o,2,1,args);a->font_dtor(font);}
}
bool ScreenControlView::bind(void*o,Event e,int i){auto*b=new(std::nothrow)Binding(this,i);if(!b)return false;retain();if(api_.listen(api_.context,o,e,event,b,released))return true;released(b);return false;}
void ScreenControlView::released(void*p){auto*b=static_cast<Binding*>(p);auto*s=b->self;delete b;s->drop();}
void ScreenControlView::event(void*p,Event e,double y){auto*b=static_cast<Binding*>(p);auto*s=b->self;s->retain();s->handle(b->index,e,y);s->drop();}
void ScreenControlView::hide(){open_=false;dragging_=false;if(popup_)api_.visible(api_.context,popup_,false);}
void ScreenControlView::show(){if(!alive_||closed_||!popup_||!model_->available())return;if(model_->kind()==ScreenControlKind::AdaptiveFps){selected_=model_->current()?0:1;accept();return;}selected_=model_->current();position_=selected_;animationTime_=0;open_=true;api_.visible(api_.context,popup_,true);arrange();}
void ScreenControlView::accept(){if(!alive_||closed_||!model_->available()||selected_<0||selected_>=model_->count())return;retain();bool ok=choose_&&choose_(context_,selected_);if(alive_&&!closed_&&ok)hide();drop();}
void ScreenControlView::handle(int index,Event event,double y){
 if(event==Event::Destroy){if(index==-10){popup_=nullptr;open_=false;}else{alive_=false;root_=nullptr;hide();if(popup_){auto*p=popup_;popup_=nullptr;api_.destroy(api_.context,p);}}return;}
 if(!alive_||closed_)return;if(event==Event::Hidden){hide();return;}
 if(!interactive_||!std::isfinite(y))return;
 if(event==Event::Click){if(index==-1)show();else if(index==-2)accept();return;}
 if(!open_||!model_->available())return;
 if(event==Event::Press){dragging_=true;moved_=false;pressY_=y;pressIndex_=selected_;pressPosition_=position_;}
 else if(event==Event::Move&&dragging_){double delta=pressY_-y;if(std::abs(delta)>8)moved_=true;double cell=(height_-style_.selectorTop-style_.selectorBottom)/style_.visibleChoices;double change=delta/cell;if(change>65)change=65;if(change< -65)change=-65;position_=pressPosition_+change;if(position_<0)position_=0;if(position_>model_->count()-1)position_=model_->count()-1;selected_=int(position_+.5);arrange();}
 else if(event==Event::Release){bool down=dragging_;dragging_=false;if(down&&!moved_&&index>=0&&index<7){int next=choiceIndex_[index];if(next>=0&&next<model_->count()){selected_=next;accept();}}}
}
void ScreenControlView::arrange(){if(!alive_||!root_||!popup_)return;auto c=api_.context;auto&s=style_;double total=s.rowHeight+s.descriptionHeight;
 api_.rect(c,root_,0,y_,width_,total);api_.rect(c,row_,s.left,0,width_-s.left-s.right,s.rowHeight);api_.rect(c,description_,s.left,s.rowHeight,width_-s.left-s.right,s.descriptionHeight);api_.rect(c,entry_,0,0,width_,total);
 api_.rect(c,popup_,0,0,overlayWidth_,height_);api_.rect(c,shade_,0,0,overlayWidth_,height_);api_.rect(c,outside_,0,0,overlayWidth_,height_);double pw=overlayWidth_-s.selectorLeft-s.selectorRight,ph=height_-s.selectorTop-s.selectorBottom,cell=ph/s.visibleChoices;
 api_.rect(c,frame_,s.selectorLeft,s.selectorTop,pw,ph);api_.rect(c,panel_,2,2,pw-4,ph-4);api_.rect(c,highlight_,0,(ph-cell)/2-2,pw-4,cell);
 // Match 4.2.0 ScaledDelegate: 0.6 row height, up to 30% larger at centre.
 for(int i=0;i<7;i++){int n=int(position_+.5)+i-3;choiceIndex_[i]=n;double offset=n-position_;
  bool visible=open_&&n>=0&&n<model_->count();api_.visible(c,labels_[i],visible);api_.visible(c,buttons_[i],visible);
  api_.rect(c,labels_[i],2,(ph-cell)/2+offset*cell,pw-8,cell);api_.rect(c,buttons_[i],2,(ph-cell)/2+offset*cell,pw-8,cell);
  if(visible){double proximity=1-std::abs(offset);double size=cell*.6*(1+.3*(proximity>0?proximity:0));api_.text(c,labels_[i],model_->label(n),size,1);
   // Reuse the original menu row font instead of constructing a default font.
   if(void*right=stock_get_object(stock_,row_,"rightText")){alignas(8)unsigned char font[16];abi_->font_ctor(font);int status=-1,flags=0;void*args[]={font,nullptr,&status,&flags};abi_->text(right,1,1,args);abi_->font_size(font,int(size));abi_->text(labels_[i],2,1,args);abi_->font_dtor(font);}
   api_.color(c,labels_[i],s.foreground,true);api_.opacity(c,labels_[i],n==selected_?1:.65);
  }
 }
}
void ScreenControlView::advance(uint64_t now){
 if(now<animationTime_){animationTime_=now;return;}uint64_t elapsed=animationTime_?now-animationTime_:0;animationTime_=now;
 if(!open_||dragging_||position_==selected_)return;
 double cell=(height_-style_.selectorTop-style_.selectorBottom)/style_.visibleChoices;
 if(!(cell>0)||!elapsed)return;
 double step=1280*(height_/768.0)*(double(elapsed>100?100:elapsed)/1000)/cell;
 double delta=selected_-position_;position_+=std::abs(delta)<=step?delta:delta>0?step:-step;arrange();
}
bool ScreenControlView::update(const ScreenControlStyle&s,double w,double h,double rowY,bool touch,double ow){if(!alive_||closed_||!popup_)return false;if(ow==0)ow=w;
 bool ok=number(w)&&w>0&&number(h)&&h>0&&number(rowY)&&number(s.rowHeight)&&s.rowHeight>=16&&number(s.left)&&number(s.right)&&s.left+s.right<w&&number(s.labelFont)&&s.labelFont>0&&number(s.valueFont)&&s.valueFont>0&&number(s.descriptionFont)&&s.descriptionFont>0&&number(s.descriptionHeight)&&number(s.selectorLeft)&&number(s.selectorRight)&&s.selectorLeft+s.selectorRight<ow-8&&number(s.selectorTop)&&number(s.selectorBottom)&&s.selectorTop+s.selectorBottom<h-8&&number(s.visibleChoices)&&s.visibleChoices>=1&&s.visibleChoices<=6;
 ok=ok&&number(ow)&&s.selectorLeft+s.selectorRight<ow-8;
 if(!ok){hide();api_.visible(api_.context,root_,false);return false;}width_=w;overlayWidth_=ow;height_=h;y_=rowY;style_=s;interactive_=touch;if(!model_->available())hide();if(selected_>=model_->count())selected_=model_->current();auto c=api_.context;
 const char16_t*value=model_->busy()?u"正在设置…":model_->error()?u"设置未确认":model_->current()>=0?model_->label(model_->current()):u"暂不可用";
 ok=stock_set_text(stock_,row_,"text",s.title?s.title:model_->title());if(model_->kind()==ScreenControlKind::AdaptiveFps){ok=stock_set_bool(stock_,row_,"value",model_->current()==1)&&stock_set_bool(stock_,row_,"itemEnabled",model_->available())&&stock_set_bool(stock_,row_,"highlighted",s.highlighted)&&stock_set_bool(stock_,row_,"showSwitch",s.showSwitch)&&stock_set_number(stock_,row_,"fontWeight",s.fontWeight)&&ok;}else{ok=stock_set_text(stock_,row_,"valueText",value)&&stock_set_number(stock_,row_,"pixelSize",s.labelFont)&&ok;void*right=stock_get_object(stock_,row_,"rightText");if(!right)ok=false;else{text_size(abi_,right,s.valueFont);api_.opacity(c,right,model_->available()?1:.35);}}
 api_.text(c,description_,s.description?s.description:model_->description(),s.descriptionFont,0);ok=stock_set_number(stock_,description_,"wrapMode",1)&&ok;api_.color(c,description_,s.foreground,true);api_.opacity(c,description_,.7);api_.visible(c,description_,s.descriptionHeight>0);api_.enabled(c,entry_,model_->available()&&touch);api_.color(c,frame_,s.border,false);api_.color(c,panel_,s.background,false);api_.color(c,highlight_,s.highlight,false);for(auto*l:labels_)api_.color(c,l,s.foreground,true);arrange();api_.visible(c,root_,ok);return ok;
}
bool ScreenControlView::key(ScreenControlKey k){if(!alive_||closed_)return false;retain();bool accepted=true;
 if(k==ScreenControlKey::Open)show();else if(k==ScreenControlKey::Abort)hide();else if(k==ScreenControlKey::Accept||k==ScreenControlKey::HalfPress){if(open_)accept();else accepted=false;}else if(k==ScreenControlKey::Navigation){}else if(open_){selected_+=k==ScreenControlKey::Up?-1:1;if(selected_<0)selected_=0;if(selected_>=model_->count())selected_=model_->count()-1;arrange();}else accepted=false;drop();return accepted;
}
ScreenControlView*ScreenControlView::create(ViewApi a,StockProfile s,AbiViewProfile*b,ScreenControl*m,void*co,void*host,void*overlay,bool(*choose)(void*,int),void*ctx){
 if(!b||!b->font_ctor||!b->font_dtor||!b->font_size||!b->text||!m||!co||!host||!overlay||!choose||!a.create||!a.destroy||!a.rect||!a.color||!a.text||!a.visible||!a.enabled||!a.opacity||!a.name||!a.layer||!a.listen)return nullptr;
 auto*v=new(std::nothrow)ScreenControlView(a,s,b,m);if(!v)return nullptr;v->choose_=choose;v->context_=ctx;auto make=[&](Kind k,void*p){return a.create(a.context,k,p);};v->root_=make(Kind::Item,host);if(!v->root_){delete v;return nullptr;}if(!v->bind(v->root_,Event::Destroy,-3)){a.destroy(a.context,v->root_);delete v;return nullptr;}auto fail=[&](){v->close();return static_cast<ScreenControlView*>(nullptr);};a.name(a.context,v->root_,"X2dNativeScreenControl");
 v->row_=stock_create(s,m->kind()==ScreenControlKind::AdaptiveFps?StockKind::MenuBoolSelector:StockKind::TextValueRow,co,v->root_);v->description_=make(Kind::Text,v->root_);v->entry_=make(Kind::Mouse,v->root_);v->popup_=make(Kind::Item,overlay);if(!v->row_||!v->description_||!v->entry_||!v->popup_)return fail();if(!v->bind(v->popup_,Event::Destroy,-10))return fail();a.name(a.context,v->popup_,"X2dNativeScreenSelector");a.layer(a.context,v->popup_,2000,false);
 a.name(a.context,v->description_,"X2dNativeScreenDescription");v->shade_=make(Kind::Rectangle,v->popup_);v->outside_=make(Kind::Mouse,v->popup_);v->frame_=make(Kind::Rectangle,v->popup_);if(!v->shade_||!v->outside_||!v->frame_)return fail();v->panel_=make(Kind::Rectangle,v->frame_);if(!v->panel_)return fail();v->highlight_=make(Kind::Rectangle,v->panel_);if(!v->highlight_)return fail();a.color(a.context,v->shade_,0,false);a.opacity(a.context,v->shade_,.7);a.layer(a.context,v->panel_,0,true);
 if(!v->bind(v->entry_,Event::Click,-1)||!v->bind(v->outside_,Event::Click,-2)||!v->bind(v->root_,Event::Hidden,-3))return fail();
 for(int i=0;i<7;i++){v->labels_[i]=make(Kind::Text,v->panel_);v->buttons_[i]=make(Kind::Mouse,v->panel_);if(!v->labels_[i]||!v->buttons_[i])return fail();char name[48];snprintf(name,sizeof name,"X2dScreenChoice%d",i);a.name(a.context,v->buttons_[i],name);if(!v->bind(v->buttons_[i],Event::Press,i)||!v->bind(v->buttons_[i],Event::Move,i)||!v->bind(v->buttons_[i],Event::Release,i))return fail();}
 v->hide();a.visible(a.context,v->root_,false);return v;
}
void ScreenControlView::close(){if(closed_)return;closed_=true;alive_=false;hide();if(root_){auto*p=root_;root_=nullptr;api_.destroy(api_.context,p);}if(popup_){auto*p=popup_;popup_=nullptr;api_.destroy(api_.context,p);}drop();}
}
