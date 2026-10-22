#include "processing_stock_view.hpp"
#include <new>
#include <cstring>
namespace x2dui {
static void array_release(const MenuProfile&p,AbiArray&a){if(a.owner){auto*r=static_cast<int*>(a.owner);if(__atomic_load_n(r,__ATOMIC_RELAXED)>=0&&__atomic_sub_fetch(r,1,__ATOMIC_ACQ_REL)==0)p.deallocate(a.owner,8,8);}}
static bool named(const MenuProfile&p,void*o,const char16_t*s){auto n=p.name(o);std::ptrdiff_t size=0;while(s[size])++size;bool ok=n.size==size&&n.data&&!std::memcmp(n.data,s,size*2);array_release(p,n);return ok;}
static bool numeric(const MenuProfile&p,void*o,const char*name,double&value){if(!o)return false;auto v=p.property(o,name);bool ok=false;value=p.number(&v,&ok);p.variant_destroy(&v);return ok&&value==value&&value>=0&&value<=8192;}
static bool pixel_size(AbiViewProfile*a,void*o,double size){if(!(size>0&&size<=512))return false;alignas(8)unsigned char font[16];a->font_ctor(font);int status=-1,flags=0;void*args[]={font,nullptr,&status,&flags};a->text(o,1,1,args);a->font_size(font,static_cast<int>(size));a->text(o,2,1,args);a->font_dtor(font);return true;}
void ProcessingStockView::release(void*p){static_cast<ProcessingStockView*>(p)->drop();}
void ProcessingStockView::event(void*p,Event e,double){auto*s=static_cast<ProcessingStockView*>(p);if(e==Event::Destroy){s->root=nullptr;s->available=false;s->area=nullptr;return;}if(e==Event::Click)s->invoke();}
void ProcessingStockView::aborted(void*p){static_cast<ProcessingStockView*>(p)->invoke();}
void ProcessingStockView::modelDestroyed(void*p){auto*s=static_cast<ProcessingStockView*>(p);s->model=nullptr;s->available=false;if(s->root)s->view.visible(s->view.context,s->root,false);}
bool ProcessingStockView::key(void*p,const KeyInput&k){auto*s=static_cast<ProcessingStockView*>(p);if(s->closed||!s->root)return false;bool consume=!key_unblockable(s->keyMap,k);if(key_matches(s->keyMap,k,KeyFunction::Abort)||key_matches(s->keyMap,k,KeyFunction::Escape)||key_matches(s->keyMap,k,KeyFunction::JstkEscape))s->invoke();return consume;}
bool ProcessingStockView::bind(void*object,Event e){retain();if(view.listen(view.context,object,e,event,this,release))return true;drop();return false;}
void ProcessingStockView::invoke(){if(closed||!root||!available||!action)return;retain();action(context,finish?ProcessingAction::Finish:ProcessingAction::Close);drop();}
void*ProcessingStockView::findArea(){
 struct Entry{void*object;unsigned depth;};Entry pending[4096];unsigned count=1,seen=0,matches=0;void*found=nullptr;pending[0]={root,0};
 while(count){Entry e=pending[--count];if(++seen>4096||e.depth>64)return nullptr;if(named(menu,e.object,u"ConfirmButtonArea")){found=e.object;++matches;}
  auto children=menu.children(e.object);bool valid=children.size>=0&&children.size<=4096&&children.size<=4096-count&&(children.data||!children.size);if(valid){auto**objects=static_cast<void**>(children.data);for(std::ptrdiff_t i=0;i<children.size;i++)pending[count++]={objects[i],e.depth+1};}array_release(menu,children);if(!valid)return nullptr;
 }return matches==1?found:nullptr;
}
ProcessingStockView*ProcessingStockView::create(StockProfile s,MenuProfile m,AbiViewProfile*a,SignalProfile g,void*contextObject,void*parent,void*factoryExposureModel,void(*action)(void*,ProcessingAction),void*context,const KeyProfile*keyProfile,const KeyMapProfile*keyMap){
 if(!a||!contextObject||!parent||!factoryExposureModel||!action)return nullptr;auto*v=new(std::nothrow)ProcessingStockView(s,m,a);if(!v)return nullptr;v->action=action;v->context=context;
 v->root=stock_create(s,StockKind::ExposeScreen,contextObject,parent);if(!v->root){v->close();return nullptr;}
 if(!v->bind(v->root,Event::Destroy)){v->close();return nullptr;}v->view.name(v->view.context,v->root,"PixelOverclockPage");
 v->model=factoryExposureModel;
 v->progress=stock_create(s,StockKind::ExposeProgress,contextObject,v->root);v->countdown=stock_create(s,StockKind::ExposeIconText,contextObject,v->root);
 if(!v->model||!v->progress||!v->countdown||!stock_set_object(s,v->root,"viewModel",v->model)){v->close();return nullptr;}
 v->retain();v->modelWatch=signal_watch(g,v->model,v->root,"destroyed(QObject*)",modelDestroyed,v,release);if(!v->modelWatch){v->drop();v->close();return nullptr;}
 v->button=stock_get_object(s,v->root,"btn2");v->countText=stock_get_object(s,v->countdown,"text1");const char*names[]={"text1","text2","text3"};for(int n=0;n<3;n++)v->fields[n]=stock_get_object(s,v->progress,names[n]);
 void*icon=stock_get_object(s,v->countdown,"icon");if(!v->button||!v->countText||!icon||!v->fields[0]||!v->fields[1]||!v->fields[2]||!stock_set_text(s,icon,"source",u"image://svg/ic_drive_mode_self_timer_big")){v->close();return nullptr;}
 v->backdrop=v->view.create(v->view.context,Kind::Rectangle,v->root);v->touch=v->view.create(v->view.context,Kind::Mouse,v->root);if(!v->backdrop||!v->touch||!v->bind(v->touch,Event::Click)){v->close();return nullptr;}
 v->view.name(v->view.context,v->touch,"PixelOverclockReturnTouch");v->view.name(v->view.context,v->progress,"PixelOverclockNativeProgress");v->view.name(v->view.context,v->countdown,"PixelOverclockNativeCountdown");
 v->view.layer(v->view.context,v->backdrop,-1,false);v->view.color(v->view.context,v->backdrop,0,false);v->view.layer(v->view.context,v->touch,201,false);
 v->retain();v->abortWatch=signal_watch(g,v->root,v->root,"abortPressed()",aborted,v,release);if(!v->abortWatch){v->drop();v->close();return nullptr;}
 v->area=v->findArea();if(!v->area){v->close();return nullptr;}if(keyProfile||keyMap){if(!keyProfile||!keyMap||!key_instance(*keyMap)){v->close();return nullptr;}v->keyMap=*keyMap;v->keys=NativeKeyFilter::create(*keyProfile,g,v->root,key,v);if(!v->keys){v->close();return nullptr;}}v->view.visible(v->view.context,v->root,false);return v;
}
bool ProcessingStockView::layout(double w,double h){
 if(!(w>0&&w<=8192&&h>0&&h<=8192)||!root)return false;width=w;height=h;
 void*items[]={root,progress,countdown,backdrop};for(void*o:items)view.rect(view.context,o,0,0,w,h);
 // The factory owns these children for the root lifetime. Never approximate
 // button bounds: use its actual background geometry and orientation.
 area=findArea();double side=0,bw=0,bh=0;void*background=area?stock_get_object(stock,area,"background"):nullptr;
 bool geometry=area&&numeric(menu,root,"sideOriented",side)&&numeric(menu,background,"width",bw)&&numeric(menu,background,"height",bh)&&bw<=w&&bh<=h;
 if(geometry){double x=side?w-bw:0,y=side?h/2:h-bh;view.rect(view.context,touch,x,y,side?bw:w/2,h-y);}
 view.enabled(view.context,touch,available&&geometry);view.visible(view.context,touch,available&&geometry);return geometry;
}
bool ProcessingStockView::update(const ProcessingState&state,double w,double h,double scale,double scaleX){
 if(closed||!root||!model)return false;if(!(w>0&&w<=8192&&h>0&&h<=8192&&scale>0&&scale<=4&&scaleX>0&&scaleX<=4)){available=false;view.enabled(view.context,touch,false);view.visible(view.context,root,false);return false;}auto p=processing_presentation(state);available=p.showButtons;finish=p.finishAllowed||p.cancelAllowed;
 bool ok=stock_set_text(stock,root,"headerText",p.title)&&stock_set_bool(stock,root,"showButtons",available)&&stock_set_bool(stock,button,"enabled",available)&&stock_set_text(stock,button,"text",p.button);
 ok=stock_set_text(stock,fields[0],"text",p.status)&&stock_set_text(stock,fields[1],"text",p.quantity)&&stock_set_text(stock,fields[2],"text",p.detail)&&stock_set_text(stock,countText,"text",p.countdown)&&ok;
 unsigned len=0;for(const auto*c=p.status;*c;c++)++len;for(const auto*c=p.quantity;*c;c++)++len;double size=len>12?80*scaleX:96*scale;
 ok=pixel_size(abi,fields[0],size)&&pixel_size(abi,fields[1],size)&&pixel_size(abi,fields[2],28.8*scale)&&stock_set_number(stock,progress,"rowSpacing",p.quantity[0]?64*scaleX:0)&&stock_set_number(stock,countdown,"iconSpacing",25.6*scaleX)&&ok;
 ok=stock_set_number(stock,progress,"progressValue",p.progress)&&stock_set_bool(stock,progress,"showInfiniteLoop",p.infinite)&&ok;
 view.visible(view.context,countdown,p.counting);view.visible(view.context,progress,!p.counting);view.visible(view.context,root,true);bool geometry=layout(w,h);if(!ok){available=false;view.enabled(view.context,touch,false);}return ok&&geometry;
}
void ProcessingStockView::close(){if(closed)return;closed=true;available=false;if(keys){keys->close();keys=nullptr;}signal_unwatch(modelWatch);modelWatch=nullptr;signal_unwatch(abortWatch);abortWatch=nullptr;if(root){auto*object=root;root=nullptr;stock.object_destroy(object);}drop();}
}
