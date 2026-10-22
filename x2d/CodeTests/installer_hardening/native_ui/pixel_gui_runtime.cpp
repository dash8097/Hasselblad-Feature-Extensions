#include "pixel_gui_runtime.hpp"
namespace x2dui {
PixelGuiRuntime::PixelGuiRuntime(StockProfile s,MenuProfile m,AbiViewProfile*a,SignalProfile g,KeyProfile k,KeyMapProfile map,FocusProfile f,PixelIoApi io,PixelFeatures features):stock(s),menu(m),api(abi_view_api(a)),controller_(features),host_(controller_),pump(controller_,io),page(s,m,a,g,k,map,f,pageAction,this){}
PixelGuiRuntime::~PixelGuiRuntime(){close(nullptr);}
void PixelGuiRuntime::pageAction(void*p,ProcessingAction action){auto*s=static_cast<PixelGuiRuntime*>(p);if(action==ProcessingAction::Finish)s->controller_.finish_stack();else s->host_.close_processing();}
void PixelGuiRuntime::bubbleEvent(void*p,Event e,double){if(e==Event::Destroy){auto*s=static_cast<PixelGuiRuntime*>(p);s->bubble=s->bubbleText=s->bubbleScene=nullptr;}}
void PixelGuiRuntime::closeBubble(){if(bubble){void*p=bubble;bubble=bubbleText=bubbleScene=nullptr;api.destroy(api.context,p);}}
static bool number(const MenuProfile&p,void*o,const char*name,double&value){auto v=p.property(o,name);bool ok=false;value=p.number(&v,&ok);p.variant_destroy(&v);return ok&&value>=0&&value<=8192;}
bool PixelGuiRuntime::showPreparation(void*root,const UiText&text,double w,double h,double scale,double sy){
 if(!root||text.empty()){closeBubble();return true;}if(bubble&&bubbleScene!=root)closeBubble();
 if(!bubble){bubble=api.create(api.context,Kind::Item,root);if(!bubble)return false;bubbleScene=root;if(!api.listen(api.context,bubble,Event::Destroy,bubbleEvent,this,release)){closeBubble();return false;}bubbleText=api.create(api.context,Kind::Text,bubble);if(!bubbleText){closeBubble();return false;}api.name(api.context,bubble,"PixelOverclockPreparationBubble");api.name(api.context,bubbleText,"PixelOverclockPreparationText");api.layer(api.context,bubble,1201,false);api.color(api.context,bubbleText,0xffffff,true);if(!stock_set_number(stock,bubbleText,"style",1)||!stock_set_text(stock,bubbleText,"styleColor",u"#000000")){closeBubble();return false;}}
 api.text(api.context,bubbleText,text.c_str(),38.4*scale,1);double tw=0,th=0;if(!number(menu,bubbleText,"implicitWidth",tw)||!number(menu,bubbleText,"implicitHeight",th)){closeBubble();return false;}api.rect(api.context,bubble,(w-tw)/2,h-160*sy-th,tw,th);api.rect(api.context,bubbleText,0,0,tw,th);return true;
}
bool PixelGuiRuntime::tick(void*root,uint64_t now,const AlbumContext&album,double w,double h,double scale,double sx,double sy){
 if(!(w>0&&w<=8192&&h>0&&h<=8192&&scale>0&&scale<=4&&sx>0&&sx<=4&&sy>0&&sy<=4)){close(root);return false;}
 pump.tick(now);host_.observe(now,album);auto state=controller_.processing_state();state.applicationError=host_.state().applicationError.c_str();bool shown=page.sync(root,host_.state().showing,state,w,h,scale,sx);
 UiText notice;if(host_.state().preparationPage&&controller_.state().preparingMode&&!controller_.state().busy)notice=host_.preparation_text();return showPreparation(root,notice,w,h,scale,sy)&&shown;
}
void PixelGuiRuntime::close(void*root){pump.suspend();page.close(root);closeBubble();}
}
