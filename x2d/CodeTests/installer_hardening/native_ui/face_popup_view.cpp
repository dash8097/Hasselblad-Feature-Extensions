#include "face_popup_view.hpp"
#include <new>
#include <cmath>
#include <cstdio>
namespace x2dui {
void FacePopupView::released(void*p){auto*b=static_cast<Binding*>(p);auto*s=b->self;delete b;s->drop();}
void FacePopupView::event(void*p,Event e,double){auto*b=static_cast<Binding*>(p);auto*s=b->self;s->retain();if(e==Event::Destroy){s->root_=nullptr;s->finished_=true;}else s->handle(b->index);s->drop();}
void FacePopupView::signal(void*p){auto*b=static_cast<Binding*>(p);auto*s=b->self;s->retain();s->handle(b->index);s->drop();}
bool FacePopupView::bind(void*o,Event e,int i){auto*b=new(std::nothrow)Binding(this,i);if(!b)return false;retain();if(api_.listen(api_.context,o,e,event,b,released))return true;released(b);return false;}
bool FacePopupView::watch(void*o,const char*n,int i,int slot){auto*b=new(std::nothrow)Binding(this,i);if(!b)return false;retain();watches_[slot]=signal_watch(signals_,o,root_,n,signal,b,released);if(watches_[slot])return true;released(b);return false;}
bool FacePopupView::selectable(int i)const{return !closed_&&!finished_&&root_&&snapshot_.attached&&snapshot_.canChange&&!snapshot_.busy&&i>=0&&i<3&&(i<2?snapshot_.multiAllowed:snapshot_.offAllowed);}
void FacePopupView::accept(){if(!selectable(selected_)||!choose_)return;retain();choose_(context_,selected_);drop();}
void FacePopupView::handle(int i){if(closed_||!root_)return;if(i==-3){finished_=true;return;}if(finished_)return;if(i==-2){if(!touch_)return;accept();if(!closed_&&root_)applied();return;}if(i>=0&&i<3&&touch_&&selectable(i)){selected_=i;refresh();accept();}}
bool FacePopupView::refresh(){if(!root_||closed_)return false;auto c=api_.context;double sx=style_.scaleX,sy=style_.scaleY,pw=848*sx,ph=368*sy,top=208*sy,x=(width_-pw)/2,cw=pw,ch=ph-style_.headerHeight;
 api_.rect(c,root_,0,0,width_,height_);api_.rect(c,content_,0,0,cw,ch);
 bool ok=stock_set_text(stock_,root_,"heading",style_.heading)&&stock_set_number(stock_,root_,"contentWidth",pw)&&stock_set_number(stock_,root_,"contentHeight",ph)&&stock_set_number(stock_,root_,"popupTopMargin",top);
 double side=115.2*sx,hw=171.2*sx,hh=113.6*sy,iw=(cw-2*side-3*hw)/2+hw,left=(cw-3*iw)/2;
 for(int i=0;i<3;i++){auto*o=choice_[i];ok=stock_set_number(stock_,o,"x",left+i*iw)&&stock_set_number(stock_,o,"y",40*sy)&&stock_set_number(stock_,o,"width",iw)&&ok;ok=stock_set_bool(stock_,o,"selectable",selectable(i))&&stock_set_bool(stock_,o,"forceHighlight",i==selected_)&&stock_set_bool(stock_,o,"focus",i==selected_)&&stock_set_number(stock_,o,"highlightWidth",hw)&&stock_set_number(stock_,o,"highlightHeight",hh)&&stock_set_number(stock_,o,"textPixelSize",28.8*style_.scale)&&stock_set_number(stock_,o,"textSideMargin",6.4*sx)&&stock_set_number(stock_,o,"textTopMargin",8*sy)&&ok;api_.enabled(c,o,selectable(i)&&touch_);}
 api_.rect(c,status_,0,ch-32*style_.scale,cw,32*style_.scale);api_.text(c,status_,snapshot_.busy?u"正在设置…":snapshot_.error?u"设置未完成，请检查当前状态后重试":u"",20*style_.scale,1);api_.color(c,status_,0xe6a02b,true);
 api_.rect(c,outside_[0],0,0,width_,top);api_.rect(c,outside_[1],0,top+ph,width_,height_-top-ph);api_.rect(c,outside_[2],0,top,x,ph);api_.rect(c,outside_[3],x+pw,top,width_-x-pw,ph);for(auto*o:outside_)api_.enabled(c,o,touch_);return ok;
}
bool FacePopupView::update(const FacePopupSnapshot&s,const FacePopupStyle&style,double w,double h,bool touch){if(!root_||closed_||finished_)return false;
 bool ok=std::isfinite(w)&&std::isfinite(h)&&w>0&&h>0&&w<=16384&&h<=16384&&std::isfinite(style.scaleX)&&std::isfinite(style.scaleY)&&std::isfinite(style.scale)&&style.scaleX>0&&style.scaleY>0&&style.scale>0&&style.scale<=8&&std::isfinite(style.headerHeight)&&style.headerHeight>0&&style.headerHeight<368*style.scaleY&&848*style.scaleX<=w&&576*style.scaleY<=h&&style.heading;
 if(ok){size_t n=0;while(n<256&&style.heading[n])++n;ok=n<256;}if(!ok){applied();return false;}
 if(s.current!=snapshot_.current||s.rawMode!=snapshot_.rawMode||s.attached!=snapshot_.attached){if(s.attached&&s.current>=0&&s.current<3)selected_=s.current;}
 snapshot_=s;style_=style;size_t n=0;do{heading_[n]=style.heading[n];}while(style.heading[n++]);style_.heading=heading_;width_=w;height_=h;touch_=touch;if(!s.canChange){applied();return true;}ok=refresh();if(!ok)applied();return ok;
}
bool FacePopupView::key(FacePopupKey k){if(!root_||closed_||finished_)return false;retain();bool used=true;if(k==FacePopupKey::Previous){if(selected_>0)--selected_;refresh();}else if(k==FacePopupKey::Next){if(selected_<2)++selected_;refresh();}else if(k==FacePopupKey::Accept)accept();else if(k==FacePopupKey::Abort)applied();else used=false;drop();return used;}
void FacePopupView::applied(){if(closed_||finished_||!root_)return;finished_=true;stock_set_text(stock_,root_,"state",u"finish");}
FacePopupView*FacePopupView::create(ViewApi a,StockProfile s,SignalProfile g,FacePopupProfile l,void*co,void*parent,bool(*choose)(void*,int),void*ctx){
 if(!co||!parent||!choose||!l.listType||!a.create||!a.destroy||!a.rect||!a.color||!a.text||!a.visible||!a.enabled||!a.name||!a.layer||!a.listen)return nullptr;
 auto*v=new(std::nothrow)FacePopupView(a,s,g,l);if(!v)return nullptr;v->choose_=choose;v->context_=ctx;v->root_=stock_create(s,StockKind::Popover,co,parent);if(!v->root_){delete v;return nullptr;}if(!v->bind(v->root_,Event::Destroy,-1)){a.destroy(a.context,v->root_);delete v;return nullptr;}auto fail=[&](){v->close();return static_cast<FacePopupView*>(nullptr);};a.name(a.context,v->root_,"X2dNativeFacePopup");a.layer(a.context,v->root_,2100,false);
 if(!stock_set_bool(s,v->root_,"handleDefaultKeys",false))return fail();v->content_=a.create(a.context,Kind::Item,v->root_);if(!v->content_)return fail();v->status_=a.create(a.context,Kind::Text,v->content_);if(!v->status_)return fail();auto variant=s.property(v->root_,"content");bool attached=false;if(s.variant_convert(&variant,{l.listType})){auto*list=static_cast<FacePopupList*>(s.variant_data(&variant));if(list&&list->object&&list->append&&list->count&&list->at&&list->count(list)==0){list->append(list,v->content_);attached=list->count(list)==1&&list->at(list,0)==v->content_;}}s.variant_destroy(&variant);if(!attached)return fail();
 const char16_t*labels[]={u"人脸",u"人脸＋人眼",u"关闭"};const char16_t*icons[]={u"image://svg/ic_face_detection_popup_multi",u"image://svg/ic_face_detection_popup_single",u"image://svg/ic_no_face_detection_popup"};
 for(int i=0;i<3;i++){v->choice_[i]=stock_create(s,StockKind::FramedImage,co,v->content_);auto*o=v->choice_[i];if(!o)return fail();char name[48];snprintf(name,sizeof name,"X2dNativeFaceChoice%d",i);a.name(a.context,o,name);if(!stock_set_bool(s,o,"acceptKeys",false)||!stock_set_text(s,o,"text",labels[i])||!stock_set_text(s,o,"source",icons[i])||!v->watch(o,"pressed()",i,i))return fail();}
 for(int i=0;i<4;i++){auto*o=a.create(a.context,Kind::Mouse,v->root_);v->outside_[i]=o;if(!o||!v->bind(o,Event::Click,-2))return fail();a.layer(a.context,o,101,false);}
 if(!v->watch(v->root_,"closePopup()",-3,3))return fail();return v;
}
void FacePopupView::close(){if(closed_)return;closed_=true;finished_=true;for(auto*&w:watches_){auto*p=w;w=nullptr;signal_unwatch(p);}if(root_){auto*p=root_;root_=nullptr;api_.destroy(api_.context,p);}drop();}
}
