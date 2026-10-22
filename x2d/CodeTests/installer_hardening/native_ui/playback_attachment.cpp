#include "playback_attachment.hpp"
namespace x2dui {
static void release(const MenuProfile&p,AbiArray&a,unsigned size){if(a.owner){auto*r=static_cast<int*>(a.owner);if(__atomic_load_n(r,__ATOMIC_RELAXED)>=0&&__atomic_sub_fetch(r,1,__ATOMIC_ACQ_REL)==0)p.deallocate(a.owner,size,8);}}
static bool number(const MenuProfile&p,void*o,const char*n,unsigned&out){auto v=p.property(o,n);bool ok=false;double x=p.number(&v,&ok);p.variant_destroy(&v);if(!ok||x<0||x>32768||x!=static_cast<unsigned>(x))return false;out=static_cast<unsigned>(x);return true;}
static ProtocolText text(const MenuProfile&p,void*o,const char*n){auto v=p.property(o,n);auto s=p.string(&v);ProtocolText result;if(s.size<0||s.size>128||(!s.data&&s.size))result.invalidate();else for(std::ptrdiff_t i=0;i<s.size;i++){auto c=static_cast<char16_t*>(s.data)[i];if(c>127){result.invalidate();break;}result+=char(c);}release(p,s,2);p.variant_destroy(&v);return result;}
bool PlaybackAttachment::locate(void*node,void*&found,unsigned&count,unsigned&visited,unsigned depth)const{
 if(!node||depth>64||++visited>4096)return false;
 if(stock_has(stock,node,"allMask")&&stock_has(stock,node,"ready")&&stock_has(stock,node,"imageSize")&&stock_has(stock,node,"overExposureRunning")&&stock_has(stock,node,"status")&&stock_has(stock,node,"source")){found=node;++count;return true;}
 auto a=menu.children(node);bool ok=a.size>=0&&a.size<=4096&&(a.data||!a.size);if(ok){auto**items=static_cast<void**>(a.data);for(std::ptrdiff_t i=0;i<a.size&&ok;i++)ok=locate(items[i],found,count,visited,depth+1);}release(menu,a,8);return ok;
}
unsigned PlaybackAttachment::tileTexture(void*node,unsigned&visited,unsigned depth)const{
 if(!node||depth>64||++visited>4096)return 0;unsigned x,y,w,h;
 if(number(menu,node,"xPos",x)&&number(menu,node,"yPos",y)&&x==0&&y==0&&number(menu,node,"width",w)&&number(menu,node,"height",h)&&w==h&&w>0)return w;
 auto a=menu.children(node);unsigned found=0;bool ok=a.size>=0&&a.size<=4096&&(a.data||!a.size);if(ok){auto**items=static_cast<void**>(a.data);for(std::ptrdiff_t i=0;i<a.size&&!found;i++)found=tileTexture(items[i],visited,depth+1);}release(menu,a,8);return found;
}
bool PlaybackAttachment::tick(void*root,uint64_t now){
 bool changed=scene!=root||tree.pending();scene=root;if(!tree.refresh(root)){viewport.close();return false;}
 void*tiled=candidate;unsigned nodes=0;
 if(changed){clearCandidate();void*preview=find_named_item(menu,root,u"FlickablePreview_root");unsigned count=0;tiled=nullptr;
  if(!preview||!locate(preview,tiled,count,nodes)||count!=1){viewport.close();return false;}
  candidate=tiled;candidateDeath=signal_watch(signals,tiled,tiled,"destroyed(QObject*)",died,this,released);if(!candidateDeath){candidate=nullptr;viewport.close();return false;}}
 if(!tiled)return false;
 int w=0,h=0;auto src=text(menu,tiled,"source");if(!src.valid()||!stock_get_size(stock,tiled,"imageSize",w,h)||w<0||h<0||!PixelViewportCore::valid_source(src,unsigned(w),unsigned(h))){viewport.close();return false;}
 if(viewport.item()!=tiled){viewport.close();nodes=0;unsigned texture=tileTexture(tiled,nodes);if(!texture||!viewport.borrow(tiled,binding,signals,texture))return false;}
 if(!viewport.observe_existing(src,unsigned(w),unsigned(h),now)){viewport.close();return false;}return true;
}
}
