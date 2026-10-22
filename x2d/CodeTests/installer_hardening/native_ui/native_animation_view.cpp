#include "native_animation_view.hpp"
#include <cstdio>
#include <cstring>
#include <cfloat>
#include <initializer_list>
namespace x2dui {
namespace {
bool finite(double v){return v==v&&v>=-DBL_MAX&&v<=DBL_MAX;}
bool coordinate(double v){return finite(v)&&v>=-100000&&v<=100000;}
bool valid(const AnimationFrame&f,double w,double h){
 if(!finite(w)||!finite(h)||w<=0||h<=0||w>16384||h>16384||!finite(f.overlayOpacity)||f.overlayOpacity<0||f.overlayOpacity>1||!coordinate(f.starX)||!coordinate(f.starY)||!coordinate(f.starRotation)||!finite(f.starScale)||f.starScale<0||f.starScale>10||!finite(f.starOpacity)||f.starOpacity<0||f.starOpacity>1)return false;
 for(const auto&l:f.leaves)for(auto p:{l.a,l.b,l.controlB,l.outerB,l.outerA,l.controlA})if(!coordinate(p.x)||!coordinate(p.y))return false;
 return true;
}
}
void NativeAnimationView::destroyed(void*p,Event,double){auto*w=static_cast<Watch*>(p);w->self->nodes[w->index]=nullptr;w->self->alive=false;}
void NativeAnimationView::released(void*p){auto*w=static_cast<Watch*>(p);auto*self=w->self;delete w;self->drop();}
bool NativeAnimationView::watch(unsigned i){auto*w=new(std::nothrow)Watch(this,i);if(!w)return false;retain();if(!api.listen(api.context,nodes[i],Event::Destroy,destroyed,w,released)){released(w);return false;}return true;}
bool NativeAnimationView::number(unsigned i,const char*n,double v){return alive&&nodes[i]&&stock_set_number(stock,nodes[i],n,v)&&alive;}
bool NativeAnimationView::box(unsigned i,double x,double y,double w,double h){return number(i,"x",x)&&number(i,"y",y)&&number(i,"width",w)&&number(i,"height",h);}
bool NativeAnimationView::metric(unsigned i,const char*n,double&v){if(!alive||!nodes[i])return false;auto q=menu.property(nodes[i],n);bool ok=false;v=menu.number(&q,&ok);menu.variant_destroy(&q);return alive&&ok&&finite(v)&&v>0&&v<=4000;}
NativeAnimationView*NativeAnimationView::create(ViewApi a,StockProfile s,MenuProfile m,AnimationImageProfile ip,SignalProfile g,void*parent){
 if(!parent||!ip.construct||!ip.complete||!ip.set_context||ip.size<16||ip.size>4096)return nullptr;
 void*engine=s.engine(parent);void*context=engine?s.context(engine):nullptr;if(!context)return nullptr;
 auto*v=new(std::nothrow)NativeAnimationView(a,s,m);if(!v)return nullptr;
 for(unsigned i=0;i<7;i++){
  void*p=i==0?parent:i==1||i==2?v->nodes[0]:v->nodes[2];
  if(i==5){v->nodes[i]=s.allocate(ip.size);if(v->nodes[i]){ip.construct(v->nodes[i],p);s.parent(v->nodes[i],p);ip.set_context(v->nodes[i],context);}}
  else v->nodes[i]=a.create(a.context,i==1?Kind::Rectangle:i>=3?Kind::Text:Kind::Item,p);
  if(!v->nodes[i]||!v->watch(i)){v->close();return nullptr;}
 }
 a.name(a.context,v->nodes[0],"X2dNativeShutterAnimation");a.visible(a.context,v->nodes[0],false);a.enabled(a.context,v->nodes[0],false);a.layer(a.context,v->nodes[0],1000,false);
 a.color(a.context,v->nodes[1],0,false);
 for(unsigned i:{3u,4u,6u}){a.text(a.context,v->nodes[i],i==3?u"Ciallo～(∠・ω< )⌒":u"☆",i==6?100:40,0);a.color(a.context,v->nodes[i],0xffffff,true);}
 a.name(a.context,v->nodes[5],"X2dNativeShutterLeaves");
 bool ok=stock_set_bool(s,v->nodes[5],"cache",false)&&stock_set_bool(s,v->nodes[5],"asynchronous",false)&&stock_set_size(s,v->nodes[5],"sourceSize",800,600)&&v->number(5,"z",2)&&v->number(6,"z",3)&&v->box(2,0,0,800,600)&&v->box(5,0,0,800,600);
 if(!ok){v->close();return nullptr;}ip.complete(v->nodes[5]);v->retain();v->statusWatch=signal_watch(g,v->nodes[5],v->nodes[0],"statusChanged(QQuickImageBase::Status)",statusChanged,v,statusReleased);if(!v->statusWatch){v->drop();v->close();return nullptr;}return v;
}
bool NativeAnimationView::buildSvg(const AnimationFrame&f){
 size_t used=0;auto append=[&](const char*fmt,auto...values){int n;if constexpr(sizeof...(values)==0)n=std::snprintf(svg+used,sizeof(svg)-used,"%s",fmt);else n=std::snprintf(svg+used,sizeof(svg)-used,fmt,values...);if(n<0||size_t(n)>=sizeof(svg)-used)return false;used+=size_t(n);return true;};
 if(!append("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"800\" height=\"600\" viewBox=\"0 0 800 600\"><g fill=\"#131313\" stroke=\"#626262\" stroke-width=\"1.2\" stroke-miterlimit=\"10\">"))return false;
 for(const auto&l:f.leaves)if(!append("<path d=\"M %.6f %.6f L %.6f %.6f Q %.6f %.6f %.6f %.6f L %.6f %.6f Q %.6f %.6f %.6f %.6f Z\"/>",l.a.x,l.a.y,l.b.x,l.b.y,l.controlB.x,l.controlB.y,l.outerB.x,l.outerB.y,l.outerA.x,l.outerA.y,l.controlA.x,l.controlA.y,l.a.x,l.a.y))return false;
 if(!append("</g></svg>"))return false;
 constexpr char16_t prefix[]=u"data:image/svg+xml;base64,";constexpr char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
 size_t out=sizeof(prefix)/2-1;std::memcpy(uri,prefix,out*2);if(out+((used+2)/3)*4+1>sizeof(uri)/2)return false;
 for(size_t i=0;i<used;i+=3){unsigned n=unsigned(static_cast<unsigned char>(svg[i]))<<16;if(i+1<used)n|=unsigned(static_cast<unsigned char>(svg[i+1]))<<8;if(i+2<used)n|=static_cast<unsigned char>(svg[i+2]);uri[out++]=alphabet[(n>>18)&63];uri[out++]=alphabet[(n>>12)&63];uri[out++]=i+1<used?alphabet[(n>>6)&63]:'=';uri[out++]=i+2<used?alphabet[n&63]:'=';}uri[out]=0;
 return true;
}
bool NativeAnimationView::apply(const AnimationFrame&f,double w,double h){
 if(!valid(f,w,h))return false;
 if(f.overlayOpacity==0){hide();return alive;}
 double mw,mh,sw,sh,bw,bh;if(!metric(3,"implicitWidth",mw)||!metric(3,"implicitHeight",mh)||!metric(4,"implicitWidth",sw)||!metric(4,"implicitHeight",sh)||!metric(6,"implicitWidth",bw)||!metric(6,"implicitHeight",bh))return false;
 double scale=w/800<h/600?w/800:h/600,top=300-(mh>sh?mh:sh)/2,left=400-(mw+sw)/2;
 if(!box(0,0,0,w,h)||!box(1,0,0,w,h)||!box(2,(w-800)/2,(h-600)/2,800,600)||!number(2,"scale",scale)||!box(3,left,top,mw,mh)||!box(4,left+mw,top,sw,sh)||!number(4,"opacity",f.inlineStar?1:0)||!box(6,f.starX-bw/2,f.starY-bh/2,bw,bh)||!number(6,"scale",f.starScale)||!number(6,"rotation",f.starRotation)||!number(6,"opacity",f.starOpacity)||!number(0,"opacity",f.overlayOpacity))return false;
 return stock_set_bool(stock,nodes[0],"visible",true)&&alive;
}
void NativeAnimationView::statusReleased(void*p){static_cast<NativeAnimationView*>(p)->drop();}
void NativeAnimationView::statusChanged(void*p){auto*v=static_cast<NativeAnimationView*>(p);if(!v->alive||v->updating)return;v->retain();v->updating=true;if(!v->imageStatus()){v->failed=true;v->hide();}v->updating=false;v->drop();}
bool NativeAnimationView::startImage(){if(!alive)return false;if(!requested)return true;if(!buildSvg(latest))return false;pending=latest;pendingWidth=latestWidth;pendingHeight=latestHeight;pendingRevision=revision;loading=true;return stock_set_text(stock,nodes[5],"source",uri)&&alive&&imageStatus();}
bool NativeAnimationView::imageStatus(){
 if(!alive)return false;if(!loading)return true;
 auto q=menu.property(nodes[5],"status");bool ok=false;double status=menu.number(&q,&ok);menu.variant_destroy(&q);if(!alive||!ok)return false;
 if(status==2)return true;if(status!=1)return false;loading=false;
 if(!apply(pending,pendingWidth,pendingHeight))return false;
 return revision==pendingRevision||!alive||!requested?alive:startImage();
}
bool NativeAnimationView::update(const AnimationFrame&f,double w,double h){if(!alive||updating||failed)return false;retain();updating=true;bool ok=valid(f,w,h);if(ok){latest=f;latestWidth=w;latestHeight=h;++revision;if(f.overlayOpacity==0)hide();else{requested=true;ok=loading?imageStatus():startImage();}}if(!ok)hide();updating=false;drop();return ok;}
void NativeAnimationView::hide(){retain();requested=false;loading=false;++revision;if(alive&&nodes[0])stock_set_bool(stock,nodes[0],"visible",false);drop();}
void NativeAnimationView::close(){if(closed)return;closed=true;alive=false;loading=false;signal_unwatch(statusWatch);statusWatch=nullptr;void*root=nodes[0];nodes[0]=nullptr;if(root)stock.object_destroy(root);drop();}
}
