#include "face_live_route.hpp"
#include <cstring>
namespace x2dui {namespace {
void release(const MenuProfile&m,AbiArray&a,unsigned size){if(a.owner){auto*r=static_cast<int*>(a.owner);if(__atomic_load_n(r,__ATOMIC_RELAXED)>=0&&__atomic_sub_fetch(r,1,__ATOMIC_ACQ_REL)==0)m.deallocate(a.owner,size,8);}}
bool named(const MenuProfile&m,void*o,const char16_t*n){auto a=m.name(o);size_t len=0;while(n[len])++len;bool ok=a.size==std::ptrdiff_t(len)&&a.data&&!std::memcmp(a.data,n,len*2);release(m,a,2);return ok;}
}
bool face_live_route_current(const StockProfile&s,const MenuProfile&m,const FaceLiveRoute&r){
 if(!r.scene||!r.indicator||!r.viewModel||!r.face||!r.touchpad||!r.overlay||!m.name||!m.children||!m.deallocate)return false;
 const char16_t*names[]={u"AfIndicator_root",u"FaceIndicator_root",u"LiveViewTouchpad_root",u"LiveViewOverlay_root"};
 void*expected[]={r.indicator,r.face,r.touchpad,r.overlay};bool found[4]{};
 struct Entry{void*node;unsigned depth;};Entry queue[2048];void*seen[2048];unsigned count=1,visited=0;queue[0]={r.scene,0};
 while(count){auto e=queue[--count];if(!e.node||e.depth>64||visited==2048)return false;for(unsigned i=0;i<visited;i++)if(seen[i]==e.node)return false;seen[visited++]=e.node;
  for(unsigned i=0;i<4;i++)if(named(m,e.node,names[i])){if(found[i]||e.node!=expected[i])return false;found[i]=true;}
  auto a=m.children(e.node);bool ok=a.size>=0&&a.size<=std::ptrdiff_t(2048-count)&&(a.data||!a.size);if(ok)for(std::ptrdiff_t i=0;i<a.size;i++)queue[count++]={static_cast<void**>(a.data)[i],e.depth+1};release(m,a,sizeof(void*));if(!ok)return false;
 }
 for(bool exists:found)if(!exists)return false;
 return stock_get_object(s,r.indicator,"viewModel")==r.viewModel;
}
}
