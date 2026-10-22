#include "effect_menu_attachment.hpp"
#include <cmath>
namespace x2dui {
bool EffectMenuAttachment::physical(void*p,const KeyInput&k,bool released){auto&s=*static_cast<EffectMenuAttachment*>(p);
 if(released){if(s.heldKey_!=k.key)return false;s.heldKey_=0;return true;}
 if(!s.view||!s.view->popupOpen())return false;
 auto matches=[&](int fn){auto*o=key_instance(s.keyMap_);return o&&s.keyMap_.is_fn(o,k.key,unsigned(k.modifiers),fn);};
 EffectKey action;
 if(matches(5)||matches(6)||matches(31)||matches(1))action=EffectKey::Abort;
 else if(matches(28)||matches(8)||matches(10)||matches(32)||matches(44))action=EffectKey::Accept;
 else if(matches(17)||matches(20)||matches(27)||matches(36)||matches(47)||matches(25))action=EffectKey::Up;
 else if(matches(16)||matches(21)||matches(26)||matches(37)||matches(48)||matches(24))action=EffectKey::Down;
 else if(matches(11)){s.view->key(EffectKey::Abort);return false;}
 else return false;
 s.heldKey_=k.key;return s.view->key(action);
}
void EffectMenuAttachment::clear_keys(){keyLease_.close();heldKey_=0;signal_unwatch(keyDeath_);keyDeath_=nullptr;if(keyReceiver_){keyReceiver_->close();keyReceiver_=nullptr;}if(keyItem_){auto*p=keyItem_;keyItem_=nullptr;api.destroy(api.context,p);}scene_=nullptr;}
void EffectMenuAttachment::update_keys(){
 if(!keys_.window||!view||!view->item()||!view->popupOpen()||!scene_){keyLease_.close();return;}
 if(!keyItem_){if(keyReceiver_){keyReceiver_->close();keyReceiver_=nullptr;}signal_unwatch(keyDeath_);keyDeath_=nullptr;keyItem_=api.create(api.context,Kind::Item,view->item());if(!keyItem_)return;api.name(api.context,keyItem_,"X2dNativeEffectKeys");keyDeath_=signal_watch(signals,keyItem_,keyItem_,"destroyed(QObject*)",key_died,this,released);keyReceiver_=ScreenKeyReceiver::create(keys_,signals,keyItem_,physical,this);if(!keyReceiver_||!keyDeath_){auto*scene=scene_;clear_keys();scene_=scene;return;}}
 void*w=keys_.window(scene_);void*f=w?keys_.active(w):nullptr;if(f&&f!=keyItem_)keyLease_.attach(f,keyItem_);else keyLease_.close();
}
bool EffectMenuAttachment::number(void*o,const char*n,double&v){
 if(!o||!stock_has(stock,o,n))return false;auto value=menu.property(o,n);bool ok=false;v=menu.number(&value,&ok);menu.variant_destroy(&value);return ok&&std::isfinite(v);
}
bool EffectMenuAttachment::update_scroll(const MenuTarget&t,double contentHeight){
 double height=0,evf=0,interactive=0;void*model=stock_get_object(stock,t.page,"viewModel");
 if(!number(list,"height",height)||!number(model,"screenEVF",evf)||!number(list,"interactive",interactive)||(evf!=0&&evf!=1))return false;
 const bool needs=evf==0&&contentHeight<=height&&contentHeight+margin>height;
 if(!needs){if(scroll.alive()&&!scroll.close()){blockedList=list;return false;}return true;}
 if(blockedList==list)return false;
 if(scroll.alive()){if(scroll.number(1))return true;scroll.close();blockedList=list;return false;}
 if(interactive==1)return true;
 return scroll.acquire(stock,binding,signals,list,u"interactive")&&scroll.number(1);
}
void EffectMenuAttachment::refresh(int mode,bool ready,bool pending){
 // While the original interactive binding is leased it cannot react to an
 // EVF switch. Read only this attached list/model, with no tree traversal.
 if(view&&view->item()&&page){MenuTarget t;t.page=page;t.list=list;if(!update_scroll(t,menu.content_height(list)))ready=false;}
 if(view)view->update(mode,ready,pending);update_keys();
}
void EffectMenuAttachment::detach(void*root){
 clear_keys();
 signal_unwatch(pageDeath);pageDeath=nullptr;page=nullptr;
 if(scroll.alive()&&!scroll.close())blockedList=list;
 // The view's root destruction signal invalidates item() before Qt destroys
 // its owning list. A reused address alone therefore cannot grant ownership.
 if(view&&view->item()&&menu_contains(menu,root,list)&&menu.bottom_margin(list)==margin)
  menu.set_bottom_margin(list,0);
 if(view)view->close();view=nullptr;list=nullptr;margin=0;
}
int EffectMenuAttachment::tick(void*root,int mode,bool ready,double width,double height,bool pending){
 if(!root||!(width>0&&height>0)){detach(root);return 1;}
 if(view&&!view->item())detach(root);
 MenuTarget target;int rc=find_exposure_menu(menu,root,target,list,margin);
 if(rc){detach(root);if(rc==1)blockedList=nullptr;return rc;}
 if(blockedList&&target.list!=blockedList)blockedList=nullptr;
 const unsigned count=1;
 if(view&&(target.list!=list||target.rowHeight*count!=margin)){detach(root);rc=find_exposure_menu(menu,root,target);if(rc)return rc;}
 if(!view){
  view=EffectView::create(api,target.content,root,choose,context);if(!view)return 7;
  scene_=root;
  list=target.list;margin=target.rowHeight*count;
  page=target.page;pageDeath=signal_watch(signals,page,view->item(),"destroyed(QObject*)",page_destroyed,this,released);
  if(!pageDeath){detach(root);return 7;}

  menu.set_bottom_margin(list,margin);
  if(menu.bottom_margin(list)!=margin){detach(root);return 8;}
 }
 double contentHeight=menu.content_height(list);
 if(!(contentHeight>=0&&contentHeight<10000000)){detach(root);return 9;}
 double origin=0;if(!number(list,"originY",origin)||!std::isfinite(origin+contentHeight)||std::fabs(origin)>10000000){detach(root);return 9;}
 if(!update_scroll(target,contentHeight)){detach(root);return 10;}
 view->style(fontLabel,fontValue,leftMargin,rightMargin,textTop);
 view->resize(width,height);view->place(origin+contentHeight,target.rowHeight);
 refresh(mode,ready,pending);return 0;
}
}
