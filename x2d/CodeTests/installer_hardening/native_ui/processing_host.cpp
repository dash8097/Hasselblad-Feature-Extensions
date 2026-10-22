#include "processing_host.hpp"
#include <cstring>
namespace x2dui {
static void array_release(const MenuProfile&p,AbiArray&a){if(a.owner){auto*r=static_cast<int*>(a.owner);if(__atomic_load_n(r,__ATOMIC_RELAXED)>=0&&__atomic_sub_fetch(r,1,__ATOMIC_ACQ_REL)==0)p.deallocate(a.owner,8,8);}}
void*find_named_item(const MenuProfile&p,void*root,const char16_t*name){
 if(!root||!name)return nullptr;std::ptrdiff_t length=0;while(name[length])++length;
 struct Entry{void*object;unsigned depth;};Entry queue[4096];unsigned count=1,seen=0,matches=0;void*found=nullptr;queue[0]={root,0};
 while(count){auto e=queue[--count];if(!e.object||++seen>4096||e.depth>64)return nullptr;auto n=p.name(e.object);if(n.size==length&&n.data&&!std::memcmp(n.data,name,length*2)){found=e.object;++matches;}array_release(p,n);
  auto a=p.children(e.object);bool ok=a.size>=0&&a.size<=4096&&a.size<=4096-count&&(a.data||!a.size);if(ok){auto**items=static_cast<void**>(a.data);for(std::ptrdiff_t i=0;i<a.size;i++)queue[count++]={items[i],e.depth+1};}array_release(p,a);if(!ok)return nullptr;
 }return matches==1?found:nullptr;
}
ProcessingHost::ProcessingHost(StockProfile s,MenuProfile m,AbiViewProfile*a,SignalProfile g,KeyProfile k,KeyMapProfile p,FocusProfile f,void(*handler)(void*,ProcessingAction),void*c):stock(s),menu(m),abi(a),signal(g),keys(k),map(p),focus(f),exposureModels(s,m,g),action(handler),context(c){}
ProcessingHost::~ProcessingHost(){close(nullptr);}
void ProcessingHost::focusDestroyed(void*p){static_cast<ProcessingHost*>(p)->returnFocus=nullptr;}
bool ProcessingHost::enabledVisible(void*object)const{bool enabled=false,visible=false;void*e[]={&enabled,nullptr,nullptr,nullptr};void*v[]={&visible,nullptr,nullptr,nullptr};abi->item(object,1,10,e);abi->item(object,1,11,v);return enabled&&visible;}
void ProcessingHost::close(void*root){
 void*restore=nullptr;if(view&&view->item()&&root&&returnFocus&&menu_contains(menu,root,returnFocus)&&enabledVisible(returnFocus)){
  void*window=focus.window(root);void*active=window?focus.active(window):nullptr;if(active&&menu_contains(menu,view->item(),active))restore=returnFocus;
 }
 signal_unwatch(returnWatch);returnWatch=nullptr;returnFocus=nullptr;scene=nullptr;
 if(view){auto*p=view;view=nullptr;p->close();}if(restore)focus.focus(restore,7);
}
bool ProcessingHost::sync(void*root,bool requested,const ProcessingState&s,double width,double height,double scale,double scaleX){
 if(!requested||!root){close(root);return !requested;}
 void*exposure=exposureModels.resolve(root);if(!exposure){close(root);return false;}
 if(view&&(scene!=root||!view->item()||stock_get_object(stock,view->item(),"viewModel")!=exposure))close(root);
 if(!view){
  if(!key_instance(map))return false;
  void*window=focus.window(root);void*old=window?focus.active(window):nullptr;
  view=ProcessingStockView::create(stock,menu,abi,signal,root,root,exposure,action,context,&keys,&map);if(!view)return false;scene=root;
  if(old&&menu_contains(menu,root,old)){returnFocus=old;returnWatch=signal_watch(signal,old,view->item(),"destroyed(QObject*)",focusDestroyed,this,release);if(!returnWatch){close(nullptr);return false;}}
  auto api=abi_view_api(abi);api.layer(api.context,view->item(),1200,false);
 }
 if(!view->update(s,width,height,scale,scaleX)){close(root);return false;}
 void*window=focus.window(root);void*active=window?focus.active(window):nullptr;if(!active||!menu_contains(menu,view->item(),active))focus.focus(view->item(),7);return true;
}
}
