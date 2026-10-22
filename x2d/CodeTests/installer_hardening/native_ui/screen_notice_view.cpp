#include "screen_notice_view.hpp"
#include <new>
#include <cmath>
namespace x2dui {
void ScreenNoticeView::event(void*p,Event e,double){if(e==Event::Destroy)static_cast<ScreenNoticeView*>(p)->object_=nullptr;}
void ScreenNoticeView::released(void*p){static_cast<ScreenNoticeView*>(p)->drop();}
ScreenNoticeView*ScreenNoticeView::create(ViewApi a,StockProfile s,void*co,void*parent){
 if(!co||!parent||!a.visible||!a.rect||!a.destroy||!a.listen||!a.layer)return nullptr;
 auto*v=new(std::nothrow)ScreenNoticeView(a,s);if(!v)return nullptr;v->object_=stock_create(s,StockKind::PopupIconText,co,parent);if(!v->object_){delete v;return nullptr;}
 ++v->refs_;if(!a.listen(a.context,v->object_,Event::Destroy,event,v,released)){--v->refs_;a.destroy(a.context,v->object_);delete v;return nullptr;}
 a.visible(a.context,v->object_,false);a.layer(a.context,v->object_,2000,false);return v;
}
bool ScreenNoticeView::update(bool show,double w,double h,const char16_t*text){
 if(closed_||!object_)return false;bool valid=std::isfinite(w)&&std::isfinite(h)&&w>0&&h>0&&w<=16384&&h<=16384&&text;
 if(valid){size_t n=0;while(n<512&&text[n])++n;valid=n<512;}
 if(!valid){api_.visible(api_.context,object_,false);return false;}
 bool ok=stock_set_text(stock_,object_,"text",text)&&stock_set_bool(stock_,object_,"showSpinner",false);api_.rect(api_.context,object_,0,0,w,h);api_.visible(api_.context,object_,show&&ok);return ok;
}
void ScreenNoticeView::close(){if(closed_)return;closed_=true;if(object_){auto*p=object_;object_=nullptr;api_.destroy(api_.context,p);}drop();}
}
