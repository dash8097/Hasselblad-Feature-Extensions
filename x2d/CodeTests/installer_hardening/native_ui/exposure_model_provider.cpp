#include "exposure_model_provider.hpp"
#include "processing_host.hpp"
namespace x2dui {
static void*main_model(const StockProfile&s,const MenuProfile&m,void*root){auto*c=find_named_item(m,root,u"ControlScreen_focusScope");auto*vm=c?stock_get_object(s,c,"viewModel"):nullptr;return vm?stock_get_object(s,vm,"main"):nullptr;}
bool ExposureModelProvider::context_available(const StockProfile&s,const MenuProfile&m,void*root){void*v=find_named_item(m,root,u"MainViewModel_root");return (v&&stock_get_object(s,v,"exposeScreen"))||main_model(s,m,root);}
void ExposureModelProvider::objectGone(void*p){auto*s=static_cast<ExposureModelProvider*>(p);s->object=nullptr;s->owned=false;}
void ExposureModelProvider::mainGone(void*p){static_cast<ExposureModelProvider*>(p)->main=nullptr;}
void ExposureModelProvider::close(){signal_unwatch(objectDeath);signal_unwatch(mainDeath);objectDeath=mainDeath=nullptr;void*o=owned?object:nullptr;scene=object=main=nullptr;owned=false;if(o)stock.object_destroy(o);}
void*ExposureModelProvider::resolve(void*root){
 void*vm=find_named_item(menu,root,u"MainViewModel_root");void*existing=vm?stock_get_object(stock,vm,"exposeScreen"):nullptr;
 if(existing){if(owned||object!=existing||scene!=root)close();scene=root;object=existing;return existing;}
 void*next=main_model(stock,menu,root);if(!root||!next){close();return nullptr;}
 if(owned&&object&&scene==root&&main==next)return object;close();scene=root;main=next;object=stock_create(stock,StockKind::ExposeViewModel,root,root);if(!object){close();return nullptr;}owned=true;
 objectDeath=signal_watch(signals,object,object,"destroyed(QObject*)",objectGone,this,released);mainDeath=signal_watch(signals,next,object,"destroyed(QObject*)",mainGone,this,released);
 if(!objectDeath||!mainDeath||!stock_set_object(stock,object,"main",next)||!object||!main){close();return nullptr;}return object;
}
}
