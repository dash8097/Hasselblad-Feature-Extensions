#include "about_row.hpp"
#include <new>
namespace x2dui {
void AboutRow::release(void*p){static_cast<AboutRow*>(p)->drop();}
void AboutRow::changed(void*p){auto*s=static_cast<AboutRow*>(p);s->retain();s->refresh();s->drop();}
void AboutRow::model_gone(void*p){static_cast<AboutRow*>(p)->model=nullptr;}
void AboutRow::label_gone(void*p){static_cast<AboutRow*>(p)->label=nullptr;}
void AboutRow::formatter_gone(void*p){static_cast<AboutRow*>(p)->formatter.slot=nullptr;}
bool AboutRow::watch(unsigned i,void*sender,const char*signature,void(*fn)(void*)){retain();watches[i]=signal_watch(signal,sender,label,signature,fn,this,release);if(watches[i])return true;drop();return false;}
bool AboutRow::text(void*object,const char*property,UiText&out){
 if(!object)return false;auto value=menu.property(object,property);auto string=menu.string(&value);bool ok=string.size>=0&&string.size<=4096&&(string.data||!string.size);UiText copy;if(ok)copy=UiText(static_cast<const char16_t*>(string.data),static_cast<size_t>(string.size));
 if(string.owner){auto*r=static_cast<int*>(string.owner);if(__atomic_load_n(r,__ATOMIC_RELAXED)>=0&&__atomic_sub_fetch(r,1,__ATOMIC_ACQ_REL)==0)menu.deallocate(string.owner,2,8);}menu.variant_destroy(&value);
 if(!ok||!copy.valid())return false;out=static_cast<UiText&&>(copy);return true;
}
bool AboutRow::formatted(UiText&out){if(!model||!label)return false;UiText current;if(!text(model,"itemName",current)||current!=name)return false;auto value=menu.property(model,"propValue");bool ok=factory_format(formatter,menu,name,value,false,out);menu.variant_destroy(&value);return ok;}
bool AboutRow::refresh(){if(closed||updating||!model||!label)return false;retain();updating=true;UiText value;bool ok=formatted(value);if(ok){const char*field=name==u"product_name"?"product_name":"model_number";auto next=about_display_text(field,true,true,original,value);if(!next.valid())ok=false;else {ok=lease.text(next.c_str());if(ok&&!closed&&label)rendered=static_cast<UiText&&>(next);else ok=false;}}updating=false;drop();return ok;}
AboutRow*AboutRow::create(StockProfile s,MenuProfile m,SignalProfile g,FactoryFormatterProfile f,BindingProfile b,void*model,void*label,bool about,bool right){
 if(!model||!label||!about||!right||!f.slot||!*f.slot)return nullptr;auto*v=new(std::nothrow)AboutRow(s,m,g,f);if(!v)return nullptr;v->model=model;v->label=label;
 auto fail=[&](){v->close();return static_cast<AboutRow*>(nullptr);};
 if(!v->text(model,"itemName",v->name)||(v->name!=u"product_name"&&v->name!=u"model_number")||!v->text(label,"text",v->original))return fail();
 bool eligible=false;for(size_t i=0;i+4<=v->original.size();i++)if(!std::memcmp(v->original.data()+i,u"100C",8)){eligible=true;break;}if(!eligible)return fail();
 if(!v->lease.acquire(s,b,g,label,u"text"))return fail();
 if(!v->watch(0,model,"destroyed(QObject*)",model_gone)||!v->watch(1,label,"destroyed(QObject*)",label_gone)||!v->watch(2,model,"propValueChanged()",changed)||!v->watch(3,label,"textChanged(QString)",changed)||!v->watch(4,*f.slot,"translationValidChanged(bool)",changed)||!v->watch(5,*f.slot,"destroyed(QObject*)",formatter_gone))return fail();
 if(!v->refresh())return fail();return v;
}
void AboutRow::close(){if(closed)return;closed=true;for(auto&w:watches){signal_unwatch(w);w=nullptr;}lease.close();model=label=nullptr;drop();}
}
