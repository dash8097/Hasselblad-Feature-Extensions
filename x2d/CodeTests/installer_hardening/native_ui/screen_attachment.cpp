#include "screen_attachment.hpp"
#include <cmath>
#include <cstring>
namespace x2dui {namespace {
void release_array(const MenuProfile&m,AbiArray&a,size_t size){if(a.owner){auto*r=static_cast<int*>(a.owner);if(__atomic_load_n(r,__ATOMIC_RELAXED)>=0&&__atomic_sub_fetch(r,1,__ATOMIC_ACQ_REL)==0)m.deallocate(a.owner,size,8);}}
const char16_t*field(ScreenControlKind k){return k==ScreenControlKind::Display?u"screens_allowed":k==ScreenControlKind::Timeout?u"screen_idle_timeout":u"allow_variable_fps";}
bool equal(const UiText&a,const char16_t*b){size_t n=0;while(b[n])n++;return a.valid()&&a.size()==n&&!std::memcmp(a.data(),b,n*2);}
}
ScreenAttachment::Row::Row(ScreenAttachment*s,ScreenControlKind k):owner(s),kind(k),model(k,{this,apply}){}
bool ScreenAttachment::Row::apply(void*p,const ScreenWrite&w){auto&r=*static_cast<Row*>(p);return r.owner->dispatch(r,w);}
bool ScreenAttachment::Row::choose(void*p,int n){auto&r=*static_cast<Row*>(p);return r.owner->choose(r,n);}
ScreenAttachment::ScreenAttachment(ViewApi a,StockProfile s,AbiViewProfile*abi,MenuProfile m,BindingProfile b,SignalProfile g,ScreenFactoryProfile f,ScreenControlStyle style):rows_{{this,ScreenControlKind::Display},{this,ScreenControlKind::Timeout},{this,ScreenControlKind::AdaptiveFps}},liveviewModel_(ScreenControlKind::LiveviewTimeout,{this,liveview_apply}),api_(a),stock_(s),abi_(abi),menu_(m),binding_(b),signals_(g),factory_(f),style_(style){}
bool ScreenAttachment::configure_keys(ScreenKeyProfile p,KeyMapProfile map){if(closed_||running_||keyReceiver_||!p.window||!p.active||!p.parent_item)return false;keyProfile_=p;keyMap_=map;return keyLease_.configure(p,signals_);}
bool ScreenAttachment::configure_liveview(LiveviewControlApi a,const char16_t*title,const char16_t*const*labels){
 if(closed_||running_||!a.read||!a.write||!title||!labels)return false;
 UiText t(title),values[7];if(!t.valid()||t.empty()||t.size()>127)return false;
 for(unsigned i=0;i<7;i++){if(!labels[i])return false;values[i]=UiText(labels[i]);if(!values[i].valid()||values[i].empty()||values[i].size()>127)return false;}
 liveviewTitle_=static_cast<UiText&&>(t);for(unsigned i=0;i<7;i++)liveviewLabels_[i]=static_cast<UiText&&>(values[i]);liveviewApi_=a;return true;
}
bool ScreenAttachment::liveview_apply(void*p,const ScreenWrite&w){auto*s=static_cast<ScreenAttachment*>(p);return !s->closed_&&s->active_&&s->liveviewApi_.write&&w.kind==ScreenWriteKind::LiveviewTimeout&&s->liveviewApi_.write(s->liveviewApi_.context,w.value,s->now_);}
bool ScreenAttachment::liveview_choose(void*p,int index){auto*s=static_cast<ScreenAttachment*>(p);if(s->closed_||s->running_||!s->active_)return false;s->running_=true;bool ok=s->refresh(s->rows_[1])&&s->liveviewModel_.choose(index,s->now_);s->running_=false;if(s->closePending_)s->shutdown();return ok;}
bool ScreenAttachment::timeout_selected()const{auto&r=rows_[1];if(!r.original||!r.visible.alive())return false;void*list=stock_get_object(stock_,r.original,"list");double index=-1,current=-2;return list&&read_number(r.original,"itemIndex",index)&&read_number(list,"currentIndex",current)&&index>=0&&index==current;}
bool ScreenAttachment::refresh_liveview(Row&r,double x,double y,double w,double){
 if(!liveviewView_){void*host=stock_get_object(stock_,r.original,"parent");if(!host)return false;liveviewView_=ScreenControlView::create(api_,stock_,abi_,&liveviewModel_,r.original,host,scene_,liveview_choose,this);if(!liveviewView_)return false;api_.name(api_.context,liveviewView_->item(),"X2dNativeLiveviewTimeout");}
 int seconds=0;bool confirmed=false;ScreenControlSnapshot snapshot;snapshot.enabled=liveviewApi_.read(liveviewApi_.context,seconds,confirmed);snapshot.liveviewSeconds=seconds;snapshot.liveviewConfirmed=confirmed;
 const char16_t*labels[7];for(unsigned i=0;i<7;i++)labels[i]=liveviewLabels_[i].data();snapshot.labels=labels;snapshot.count=7;
 if(!liveviewModel_.update(snapshot))return false;liveviewModel_.tick(now_);
 if(!r.height.alive()&&!r.height.acquire(stock_,binding_,signals_,r.original,u"height"))return false;
 if(!r.height.number(style_.rowHeight*2)||!read_number(r.original,"y",y))return false;
 bool selected=timeout_selected();if(selected&&!liveviewSelected_)liveviewLower_=lastDirection_<0;liveviewSelected_=selected;
 UiText title;void*model=stock_get_object(stock_,r.original,"model");if(!model||!read_text(model,"text1",title))return false;
 ScreenControlStyle style=style_;style.description=u"";style.descriptionHeight=0;style.title=title.data();style.highlighted=selected&&liveviewLower_;
 if(!r.view->update(style,w,height_,y+style.rowHeight,touch_,width_))return false;
 api_.rect(api_.context,r.view->item(),x,y+style.rowHeight,w,style.rowHeight);
 style.title=liveviewTitle_.data();style.highlighted=selected&&!liveviewLower_;liveviewView_->advance(now_);
 if(!liveviewView_->update(style,w,height_,y,touch_,width_))return false;api_.rect(api_.context,liveviewView_->item(),x,y,w,style.rowHeight);return true;
}
bool ScreenAttachment::physical(void*p,const KeyInput&k,bool released){auto*s=static_cast<ScreenAttachment*>(p);if(released||!s->active_||s->closed_)return false;bool popup=false;for(auto&r:s->rows_)if(r.view&&r.view->popup_open())popup=true;
 auto matches=[&](int fn){auto*o=key_instance(s->keyMap_);return o&&s->keyMap_.is_fn(o,k.key,unsigned(k.modifiers),fn);};
 if(s->liveviewView_&&s->liveviewView_->popup_open())popup=true;
 if(!popup){int direction=(matches(17)||matches(20)||matches(27)||matches(36)||matches(47))?-1:(matches(16)||matches(21)||matches(26)||matches(37)||matches(48))?1:0;
  if(direction){s->lastDirection_=direction;if(s->liveviewView_&&s->timeout_selected()){if((direction>0&&!s->liveviewLower_)||(direction<0&&s->liveviewLower_)){s->liveviewLower_=direction>0;return true;}}return false;}
  return matches(8)||matches(10)||matches(32)||matches(44)?s->key(ScreenControlKey::Open):false;
 }
 if(matches(5)||matches(6)||matches(31))return s->key(ScreenControlKey::Abort);
 if(matches(28)||matches(8)||matches(10)||matches(32)||matches(44))return s->key(ScreenControlKey::Accept);
 if(matches(11))return s->key(ScreenControlKey::HalfPress);
 if(matches(17)||matches(20)||matches(27)||matches(36)||matches(47))return s->key(ScreenControlKey::Up);
 if(matches(16)||matches(21)||matches(26)||matches(37)||matches(48))return s->key(ScreenControlKey::Down);
 if(matches(29)||matches(33))return s->key(ScreenControlKey::Navigation);return false;
}
void ScreenAttachment::key_died(void*p){static_cast<ScreenAttachment*>(p)->keyItem_=nullptr;}
void ScreenAttachment::clear_keys(){keyLease_.close();signal_unwatch(keyDeath_);keyDeath_=nullptr;if(keyReceiver_){keyReceiver_->close();keyReceiver_=nullptr;}if(keyItem_){auto*p=keyItem_;keyItem_=nullptr;api_.destroy(api_.context,p);}}
bool ScreenAttachment::update_keys(){if(!keyProfile_.window)return true;if(!active_){keyLease_.close();return true;}if(!keyItem_){clear_keys();keyItem_=api_.create(api_.context,Kind::Item,scene_);if(!keyItem_)return false;api_.name(api_.context,keyItem_,"X2dNativeScreenKeys");keyDeath_=signal_watch(signals_,keyItem_,keyItem_,"destroyed(QObject*)",key_died,this,release);keyReceiver_=ScreenKeyReceiver::create(keyProfile_,signals_,keyItem_,physical,this);if(!keyReceiver_||!keyDeath_){clear_keys();return false;}}
 void*window=keyProfile_.window(scene_);void*focus=window?keyProfile_.active(window):nullptr;if(!focus||focus==keyItem_)return keyLease_.close();return keyLease_.attach(focus,keyItem_);
}
bool ScreenAttachment::named(void*o,const char16_t*name)const{auto a=menu_.name(o);size_t n=0;while(name[n])n++;bool ok=a.size==std::ptrdiff_t(n)&&a.data&&!std::memcmp(a.data,name,n*2);release_array(menu_,a,2);return ok;}
bool ScreenAttachment::read_number(void*o,const char*n,double&x)const{if(!o||!stock_has(stock_,o,n))return false;auto v=menu_.property(o,n);bool ok=false;x=menu_.number(&v,&ok);menu_.variant_destroy(&v);return ok&&std::isfinite(x);}
bool ScreenAttachment::read_text(void*o,const char*n,UiText&text)const{if(!o||!stock_has(stock_,o,n))return false;auto v=menu_.property(o,n);auto a=menu_.string(&v);bool ok=a.size>=0&&a.size<=127&&(a.data||!a.size);if(ok)text=UiText(static_cast<const char16_t*>(a.data),size_t(a.size));release_array(menu_,a,2);menu_.variant_destroy(&v);return ok&&text.valid();}
void ScreenAttachment::changed(void*p){static_cast<Watch*>(p)->owner->dirty_=true;}
void ScreenAttachment::destroyed(void*p){auto*w=static_cast<Watch*>(p);auto*s=w->owner;if(s->drawer_==w->object)s->drawer_=nullptr;w->object=nullptr;s->dirty_=true;}
void ScreenAttachment::sceneDestroyed(void*p){auto*s=static_cast<ScreenAttachment*>(p);s->scene_=s->drawer_=nullptr;s->dirty_=true;}
bool ScreenAttachment::owned(void*o)const{auto a=menu_.name(o);bool yes=a.data&&a.size>=9&&!std::memcmp(a.data,u"X2dNative",18);release_array(menu_,a,2);return yes;}
void ScreenAttachment::clear_watches(){for(unsigned i=0;i<watchesCount_;i++){for(auto*&s:watches_[i].signals){signal_unwatch(s);s=nullptr;}watches_[i].object=nullptr;}watchesCount_=0;freeWatch_=nullptr;for(auto&b:buckets_)b=nullptr;}
bool ScreenAttachment::watch(void*o){
 const auto key=(reinterpret_cast<uintptr_t>(o)>>4)%1024;
 for(auto*w=buckets_[key];w;w=w->hashNext)if(w->object==o){if(w->seen)return false;w->seen=true;return true;}
 Watch*slot=freeWatch_;if(slot)freeWatch_=slot->freeNext;
 else{if(watchesCount_==MaxNodes)return false;slot=&watches_[watchesCount_++];}
 slot->freeNext=nullptr;slot->hashNext=buckets_[key];buckets_[key]=slot;
 auto&w=*slot;for(auto*&s:w.signals){signal_unwatch(s);s=nullptr;}w.owner=this;w.object=o;w.seen=true;
 const char*names[]={"destroyed(QObject*)","childrenChanged()","objectNameChanged(QString)","loaded()","itemChanged()","modelChanged()","mainStateChanged()"};
 for(unsigned i=0;i<7;i++){w.signals[i]=signal_watch(signals_,o,scene_,names[i],i?changed:destroyed,&w,release);if(w.signals[i])++connections_;}
 return w.signals[0]&&w.signals[1];
}
void ScreenAttachment::detach(Row&r){if(r.kind==ScreenControlKind::Timeout){liveviewModel_.close();if(liveviewView_){liveviewView_->close();liveviewView_=nullptr;}liveviewSelected_=liveviewLower_=false;}r.model.close();if(r.view){r.view->close();r.view=nullptr;}bool ea=r.enabled.alive(),va=r.visible.alive(),ha=r.height.alive();bool c=r.height.close(),a=r.enabled.close(),b=r.visible.close();if((ea&&!a)||(va&&!b)||(ha&&!c)){error_=12;r.blockedOriginal=r.original;}r.loader=r.original=nullptr;}
void ScreenAttachment::detach_rows(){for(auto&r:rows_)detach(r);}
bool ScreenAttachment::attach(Row&r,void*loader,void*original){if(original==r.blockedOriginal)return false;if(r.view&&r.original==original&&r.loader==loader&&r.visible.alive()&&r.enabled.alive())return true;detach(r);
 // HblLoader's item is visually owned by its centered inner Loader. Using
 // the outer wrapper adds its half-screen offset to the original x/y.
 void*host=stock_get_object(stock_,original,"parent");
 if(!host||!stock_.cast(stock_.meta_item,host)||!menu_contains(menu_,loader,host))return false;
 if(!r.visible.acquire(stock_,binding_,signals_,original,u"visible")||!r.enabled.acquire(stock_,binding_,signals_,original,u"enabled")){detach(r);return false;}
 if(!r.visible.number(0)||!r.enabled.number(0)){detach(r);return false;}
 r.loader=loader;r.original=original;r.view=ScreenControlView::create(api_,stock_,abi_,&r.model,original,host,scene_,Row::choose,&r);if(!r.view){detach(r);return false;}return true;
}
bool ScreenAttachment::scan(void*root){
 ++scans_;for(auto&b:buckets_)b=nullptr;freeWatch_=nullptr;
 for(unsigned i=0;i<watchesCount_;++i){auto&w=watches_[i];w.seen=false;if(w.object){auto key=(reinterpret_cast<uintptr_t>(w.object)>>4)%1024;w.hashNext=buckets_[key];buckets_[key]=&w;}else{w.freeNext=freeWatch_;freeWatch_=&w;}}
 struct Entry{void*p;unsigned depth;};Entry pending[MaxNodes];unsigned count=1,visited=0;pending[0]={root,0};void*foundLoader[3]{},*foundItem[3]{};bool ok=true;
 while(count&&ok){auto e=pending[--count];if(!e.p||e.depth>64||++visited>MaxNodes){ok=false;break;}
  if(owned(e.p))continue;
  if(!watch(e.p)){ok=false;break;}
  if(named(e.p,u"specialItem")){void*item=stock_get_object(stock_,e.p,"item");void*model=item?stock_get_object(stock_,item,"model"):nullptr;UiText name;if(model&&read_text(model,"itemName",name)){for(unsigned i=0;i<3;i++)if(equal(name,field(rows_[i].kind))){if(foundItem[i])ok=false;foundLoader[i]=e.p;foundItem[i]=item;}}}
  auto a=menu_.children(e.p);bool valid=a.size>=0&&a.size<=MaxNodes-count&&(a.data||!a.size);if(valid){auto**items=static_cast<void**>(a.data);for(std::ptrdiff_t i=0;i<a.size;i++)pending[count++]={items[i],e.depth+1};}release_array(menu_,a,8);if(!valid)ok=false;
 }
 if(!ok){clear_watches();drawer_=nullptr;attempts_=0;nextSearch_=0;detach_rows();return false;}
 // Slots remain stable because signal callbacks borrow Watch*.
 for(unsigned i=0;i<watchesCount_;++i){auto&w=watches_[i];if(!w.seen||!w.object){for(auto*&s:w.signals){signal_unwatch(s);s=nullptr;}w.object=nullptr;}}
 for(unsigned i=0;i<3;i++){if(!foundItem[i])detach(rows_[i]);else if(!attach(rows_[i],foundLoader[i],foundItem[i]))ok=false;}return ok;
}
bool ScreenAttachment::refresh(Row&r){if(!r.view)return true;if(!r.view->item()||!r.visible.alive()||!r.enabled.alive()){detach(r);dirty_=true;return false;}
 double visible=1;if(!read_number(r.original,"visible",visible)||visible!=0){detach(r);dirty_=true;return false;}
 void*model=stock_get_object(stock_,r.original,"model");UiText name,title,description;double allowed=0,proxy=-1,w=0,h=0,x=0,y=0;
 if(!model||!read_text(model,"itemName",name)||!equal(name,field(r.kind))||!read_number(model,"proxy",proxy)||proxy!=(r.kind==ScreenControlKind::AdaptiveFps?factory_.cameraProxy:factory_.systemProxy)||!read_number(model,"itemEnabled",allowed)||(allowed!=0&&allowed!=1)||!read_text(model,"text1",title)||!read_number(r.original,"width",w)||!read_number(r.original,"height",h)||!read_number(r.original,"x",x)||!read_number(r.original,"y",y)||x<0||x+w>width_||w<=0||h<style_.rowHeight||h>height_){detach(r);return false;}
 if(!screen_factory_snapshot(factory_,stock_,r.kind,allowed!=0,r.snapshot)||!r.model.update(r.snapshot.state)){detach(r);return false;}r.model.tick(now_);
 ScreenControlStyle style=style_;style.title=title.data();style.descriptionHeight=h-style.rowHeight;
 if(stock_has(stock_,model,"description")){if(!read_text(model,"description",description)){detach(r);return false;}if(r.kind!=ScreenControlKind::AdaptiveFps)style.description=description.data();}
 r.view->advance(now_);bool ok=r.view->update(style,w,height_,y,touch_,width_);
 // Stock allow_variable_fps has no description and a one-row height. Reserve
 // real wrapped text height through its original delegate so ListView lays
 // out the following section correctly; restore its binding on detach.
 if(ok&&r.kind==ScreenControlKind::AdaptiveFps){double textHeight=0;if(!read_number(r.view->description_item(),"implicitHeight",textHeight)||textHeight<=0||textHeight>height_-style.rowHeight)return false;
  double wanted=style.rowHeight+textHeight+style.descriptionFont*.4;
  if(!r.height.alive()&&!r.height.acquire(stock_,binding_,signals_,r.original,u"height"))return false;
  if(!r.height.number(wanted)||!read_number(r.original,"y",y))return false;
  h=wanted;style.descriptionHeight=wanted-style.rowHeight;ok=r.view->update(style,w,height_,y,touch_,width_);
 }
 if(ok&&r.kind==ScreenControlKind::Timeout&&liveviewApi_.read)return refresh_liveview(r,x,y,w,h);if(ok)api_.rect(api_.context,r.view->item(),x,y,w,h);return ok;
}
bool ScreenAttachment::dispatch(Row&r,const ScreenWrite&w){if(closed_||!active_||!r.visible.alive()||!r.original)return false;void*model=stock_get_object(stock_,r.original,"model");double allowed=0,proxy=-1;UiText name;if(!model||!read_number(model,"itemEnabled",allowed)||allowed!=1||!read_number(model,"proxy",proxy)||proxy!=(r.kind==ScreenControlKind::AdaptiveFps?factory_.cameraProxy:factory_.systemProxy)||!read_text(model,"itemName",name)||!equal(name,field(r.kind)))return false;return screen_factory_write(factory_,stock_,r.kind,w);}
bool ScreenAttachment::choose(Row&r,int n){if(closed_||running_||!active_)return false;running_=true;bool ok=refresh(r)&&r.model.choose(n,now_);running_=false;if(closePending_)shutdown();return ok;}
int ScreenAttachment::tick(void*root,uint64_t now,double w,double h,bool touch){if(closed_||running_)return 1;if(now<now_||!root||!std::isfinite(w)||!std::isfinite(h)||w<=0||h<=0||w>16384||h>16384)return 2;running_=true;now_=now;width_=w;height_=h;error_=0;
 if(scene_!=root){clear_keys();detach_rows();clear_watches();signal_unwatch(sceneDeath_);sceneDeath_=nullptr;scene_=root;drawer_=nullptr;attempts_=0;nextSearch_=0;dirty_=true;sceneDeath_=signal_watch(signals_,scene_,scene_,"destroyed(QObject*)",sceneDestroyed,this,release);if(!sceneDeath_)error_=3;}
 if(!error_&&!drawer_&&now>=nextSearch_&&attempts_<80){++attempts_;nextSearch_=now>UINT64_MAX-250?UINT64_MAX:now+250;struct Entry{void*p;unsigned depth;};Entry pending[MaxNodes];unsigned count=1,seen=0,hits=0;pending[0]={root,0};void*found=nullptr;while(count){auto e=pending[--count];if(++seen>MaxNodes||e.depth>64){error_=4;break;}if(owned(e.p))continue;if(named(e.p,u"ControlDrawer_root")){found=e.p;++hits;continue;}auto a=menu_.children(e.p);bool valid=a.size>=0&&a.size<=MaxNodes-count&&(a.data||!a.size);if(valid){auto**items=static_cast<void**>(a.data);for(std::ptrdiff_t i=0;i<a.size;i++)pending[count++]={items[i],e.depth+1};}release_array(menu_,a,8);if(!valid){error_=4;break;}}if(hits==1&&!error_){drawer_=found;dirty_=true;clear_watches();if(!watch(drawer_)){drawer_=nullptr;error_=5;}}else if(hits>1)error_=4;}
 UiText state;bool menu=drawer_&&read_text(drawer_,"mainState",state)&&equal(state,u"main_menu");active_=menu;touch_=touch;
 if(!menu||error_){detach_rows();if(!menu)for(auto&r:rows_)r.blockedOriginal=nullptr;}else{if(dirty_){dirty_=false;if(!scan(drawer_))error_=6;}if(!error_)for(auto&r:rows_)if(!refresh(r))error_=7;}
 if(!error_&&!update_keys())error_=8;
 running_=false;if(closePending_)shutdown();return error_;
}
bool ScreenAttachment::key(ScreenControlKey k){if(closed_||running_||!active_)return false;if(liveviewView_&&liveviewView_->popup_open())return liveviewView_->key(k);if(k==ScreenControlKey::Open&&liveviewView_&&timeout_selected()&&!liveviewLower_)return liveviewView_->key(k);for(auto&r:rows_)if(r.view&&r.view->popup_open())return r.view->key(k);if(k!=ScreenControlKey::Open)return false;for(auto&r:rows_){if(!r.view||!r.visible.alive())continue;void*list=stock_get_object(stock_,r.original,"list");double index=-1,current=-2;if(list&&read_number(r.original,"itemIndex",index)&&read_number(list,"currentIndex",current)&&index>=0&&index==current&&refresh(r))return r.view->key(k);}return false;}
void ScreenAttachment::shutdown(){closePending_=false;clear_keys();detach_rows();clear_watches();signal_unwatch(sceneDeath_);sceneDeath_=nullptr;scene_=drawer_=nullptr;active_=false;}
void ScreenAttachment::close(){if(closed_)return;closed_=true;closePending_=true;if(!running_)shutdown();}
unsigned ScreenAttachment::attached()const noexcept{unsigned n=0;for(auto&r:rows_)if(r.view&&r.view->item())++n;return n;}
}
