#include "face_draw_pool.hpp"
#include <new>
#include <cmath>
namespace x2dui {
static bool integer(const StockProfile&s,void*o,const char*n,int value){alignas(8)unsigned char v[32];s.variant_int(v,value);bool ok=s.set(o,n,v);s.variant_destroy(v);return ok;}
void FaceDrawPool::died(void*p,Event e,double){if(e==Event::Destroy){auto*s=static_cast<FaceDrawPool*>(p);s->root_=nullptr;s->allocated_=0;}}
void FaceDrawPool::released(void*p){static_cast<FaceDrawPool*>(p)->drop();}
bool FaceDrawPool::font(void*o,int size){if(!colors_.font_size)return false;auto v=stock_.property(o,"font");void*d=stock_.variant_data(&v);if(d)colors_.font_size(d,size);bool ok=d&&stock_.set(o,"font",&v);stock_.variant_destroy(&v);return ok;}
void FaceDrawPool::node_died(void*p,Event e,double){if(e==Event::Destroy)static_cast<Node*>(p)->object=nullptr;}
void FaceDrawPool::node_released(void*p){static_cast<Node*>(p)->owner->drop();}
bool FaceDrawPool::color(void*o,const char*n,uint32_t rgba){if(!colors_.colorType||!stock_has(stock_,o,n))return false;AbiColor c{1,static_cast<unsigned short>((rgba&255)*257),static_cast<unsigned short>(((rgba>>24)&255)*257),static_cast<unsigned short>(((rgba>>16)&255)*257),static_cast<unsigned short>(((rgba>>8)&255)*257),0};alignas(8)unsigned char v[32];stock_.variant_typed(v,{colors_.colorType},&c);bool ok=stock_.set(o,n,v);stock_.variant_destroy(v);return ok;}
FaceDrawPool*FaceDrawPool::create(ViewApi a,StockProfile s,FaceColorProfile c,void*p){if(!p||!c.colorType||!c.font_size||!a.create||!a.destroy||!a.visible||!a.name||!a.listen)return nullptr;auto*v=new(std::nothrow)FaceDrawPool(a,s,c);if(!v)return nullptr;Mutation hold(v);auto fail=[&](){v->close();return static_cast<FaceDrawPool*>(nullptr);};v->root_=a.create(a.context,Kind::Item,p);if(!v->root_)return fail();++v->refs_;if(!a.listen(a.context,v->root_,Event::Destroy,died,v,released)){--v->refs_;return fail();}a.name(a.context,v->root_,"X2dNativeFaceDrawing");if(!v->root_||v->closed_)return fail();if(!stock_set_number(s,v->root_,"z",0)||!v->root_||v->closed_)return fail();if(!stock_set_bool(s,v->root_,"clip",true)||!v->root_||v->closed_)return fail();a.visible(a.context,v->root_,false);if(!v->root_||v->closed_)return fail();return v;}
void FaceDrawPool::clear(){if(root_)api_.visible(api_.context,root_,false);}
bool FaceDrawPool::update(const FaceFrame&f){if(!root_||closed_||busy_)return false;Mutation hold(this);bool ok=f.valid&&f.count<=560&&std::isfinite(f.width)&&std::isfinite(f.height)&&f.width>0&&f.height>0&&f.width<=16384&&f.height<=16384;
 for(unsigned i=0;ok&&i<f.count;i++){auto&d=f.commands[i];ok=std::isfinite(d.x)&&std::isfinite(d.y)&&std::abs(d.x)<=65536&&std::abs(d.y)<=65536&&std::isfinite(d.width)&&std::isfinite(d.height)&&std::isfinite(d.radius)&&std::isfinite(d.border)&&d.width>=0&&d.height>=0&&d.width<=65536&&d.height<=65536&&d.radius>=0&&d.radius<=65536&&d.border>=0&&d.border<=64;if(d.kind==FaceDrawKind::Text)ok=ok&&d.text&&d.length<=256&&d.pixels>0&&d.pixels<=256&&d.text[d.length]==0;else ok=ok&&(d.kind==FaceDrawKind::Rectangle||d.kind==FaceDrawKind::RoundedRectangle);}
 if(!ok){clear();return false;}clear();if(!root_||closed_)return false;auto c=api_.context;
 const char*geometry[]={"x","y","width","height"};double rootGeo[]={0,0,f.width,f.height};for(unsigned n=0;n<4;n++){if(!stock_set_number(stock_,root_,geometry[n],rootGeo[n])||!root_||closed_)return false;}
 for(unsigned i=0;i<f.count;i++){auto&d=f.commands[i];auto&n=nodes_[i];bool text=d.kind==FaceDrawKind::Text;if(n.object&&(n.kind==FaceDrawKind::Text)!=text){api_.destroy(c,n.object);n.object=nullptr;if(!root_||closed_)return false;}
  if(!n.object){n.object=api_.create(c,text?Kind::Text:Kind::Rectangle,root_);if(!n.object)return false;n.owner=this;++refs_;if(!api_.listen(c,n.object,Event::Destroy,node_died,&n,node_released)){--refs_;api_.destroy(c,n.object);n.object=nullptr;return false;}}if(i>=allocated_)allocated_=i+1;n.kind=d.kind;auto*o=n.object;auto alive=[&](){return root_&&!closed_&&n.object==o;};double geo[]={d.x,d.y,d.width,d.height};for(unsigned k=0;k<4;k++)if(!stock_set_number(stock_,o,geometry[k],geo[k])||!alive())return false;
  if(text){if(!stock_set_text(stock_,o,"text",d.text)||!alive()||!font(o,d.pixels)||!alive()||!integer(stock_,o,"horizontalAlignment",4)||!alive()||!integer(stock_,o,"verticalAlignment",32)||!alive()||!color(o,"color",d.fill)||!alive())return false;
  }else{if(!color(o,"color",d.fill)||!alive()||!stock_set_number(stock_,o,"radius",d.radius)||!alive())return false;auto*pen=stock_get_object(stock_,o,"border");if(!pen||!alive()||!stock_set_number(stock_,pen,"width",d.border)||!alive()||!color(pen,"color",d.stroke)||!alive())return false;}
  api_.visible(c,o,true);if(!alive())return false;
 }
 for(unsigned i=f.count;i<allocated_;i++)if(nodes_[i].object){api_.visible(c,nodes_[i].object,false);if(!root_||closed_)return false;}api_.visible(c,root_,true);return root_&&!closed_;
}
void FaceDrawPool::shutdown(){if(finalized_)return;finalized_=true;if(root_){auto*p=root_;root_=nullptr;api_.destroy(api_.context,p);}drop();}
void FaceDrawPool::close(){if(closed_)return;closed_=true;if(!busy_)shutdown();}
}
