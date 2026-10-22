#include "abi_view_api.hpp"
#include <cstdlib>
#include <cstring>
namespace x2dui {
static_assert(sizeof(AbiString)==24&&sizeof(AbiStringView)==16&&sizeof(AbiColor)==16&&sizeof(AbiPoint)==16&&sizeof(AbiConnection)==8,"Qt scalar ABI");
struct AbiSlot {int ref;void(*impl)(int,AbiSlot*,void*,void**,bool*);AbiViewProfile*api;void*object;Event event;Handler handler;void*data;void(*release)(void*);};
static_assert(offsetof(AbiSlot,impl)==8,"Qt slot ABI");
static void property(AbiMetaCall call,void*object,int index,void*value,bool read=false){int status=-1,flags=0;void*args[]={value,nullptr,&status,&flags};call(object,read?1:2,index,args);}
static void impl(int operation,AbiSlot*s,void*,void**args,bool*equal){
 if(operation==0){if(s->release)s->release(s->data);std::free(s);return;}
 if(operation==2){if(equal)*equal=false;return;}if(operation!=1)return;
 if(s->event==Event::Hidden){bool visible=false;property(s->api->item,s->object,11,&visible,true);if(visible)return;}
 double y=0;
 if(s->event==Event::Press||s->event==Event::Move||s->event==Event::Release){if(!args||!args[1])return;void*event=*static_cast<void**>(args[1]);if(!event)return;property(s->api->mouse_event,event,1,&y,true);y=s->api->map_scene(s->object,AbiPoint{0,y}).y;}
 s->handler(s->data,s->event,y);
}
static AbiViewProfile*api(void*c){return static_cast<AbiViewProfile*>(c);}
static void*create(void*c,Kind k,void*parent){auto*a=api(c);unsigned n=static_cast<unsigned>(k);if(n>3||!parent)return nullptr;void*p=a->allocate(a->sizes[n]);if(!p)return nullptr;a->construct[n](p,parent);a->parent(p,parent);a->complete[n](p);return p;}
static void destroy(void*c,void*p){if(p)api(c)->destroy(p);}
static void rect(void*c,void*p,double x,double y,double w,double h){auto fn=api(c)->item;property(fn,p,4,&x);property(fn,p,5,&y);property(fn,p,7,&w);property(fn,p,8,&h);}
static void color(void*c,void*p,unsigned rgb,bool text){AbiColor value{1,65535,static_cast<unsigned short>(((rgb>>16)&255)*257),static_cast<unsigned short>(((rgb>>8)&255)*257),static_cast<unsigned short>((rgb&255)*257),0};auto*a=api(c);property(text?a->text:a->rectangle,p,text?2:0,&value);}
static void text(void*c,void*p,const char16_t*data,double px,int alignment){auto*a=api(c);std::ptrdiff_t count=0;while(data[count])++count;auto value=abi_owned_string(*a,data,count);property(a->text,p,0,&value);abi_release_string(*a,value);
 alignas(8) unsigned char font[16];a->font_ctor(font);a->font_size(font,static_cast<int>(px));property(a->text,p,1,font);a->font_dtor(font);
 int h=alignment==1?4:alignment==2?2:1,v=128;property(a->text,p,6,&h);property(a->text,p,8,&v);
}
static void visible(void*c,void*p,bool v){property(api(c)->item,p,11,&v);}
static void enabled(void*c,void*p,bool v){property(api(c)->item,p,10,&v);}
static void opacity(void*c,void*p,double v){property(api(c)->item,p,9,&v);}
static void name(void*c,void*p,const char*s){api(c)->set_name(p,{s,std::strlen(s)});}
static void layer(void*c,void*p,double z,bool clip){property(api(c)->item,p,6,&z);property(api(c)->item,p,27,&clip);}
static bool listen(void*c,void*p,Event event,Handler handler,void*data,void(*release)(void*)){
 auto*a=api(c);int index=-1;const void*meta=a->meta_mouse;
 switch(event){case Event::Click:index=0;break;case Event::Press:index=1;break;case Event::Move:index=2;break;case Event::Release:index=3;break;case Event::Destroy:index=4;meta=a->meta_object;break;case Event::Hidden:index=5;meta=a->meta_item;break;default:return false;}
 auto*s=static_cast<AbiSlot*>(std::calloc(1,sizeof(AbiSlot)));if(!s)return false;*s={1,impl,a,p,event,handler,data,nullptr};
 void*member[]={a->signal_functions[index],nullptr};
 // The original connectImpl consumes slot even when the connection fails.
 // Keep ownership of data local until its connection result is known.
 AbiConnection result=a->connect(p,member,p,nullptr,s,1,nullptr,meta);bool ok=result.data!=nullptr;
 if(ok)s->release=release;a->connection_dtor(&result);return ok;
}
ViewApi abi_view_api(AbiViewProfile*p){return {p,create,destroy,rect,color,text,visible,enabled,opacity,name,layer,listen};}
}
