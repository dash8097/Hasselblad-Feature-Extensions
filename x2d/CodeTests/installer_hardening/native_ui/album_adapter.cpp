#include "album_adapter.hpp"
#include "processing_host.hpp"
namespace x2dui {
static void release(const MenuProfile&p,AbiArray&a){if(a.owner){auto*r=static_cast<int*>(a.owner);if(__atomic_load_n(r,__ATOMIC_RELAXED)>=0&&__atomic_sub_fetch(r,1,__ATOMIC_ACQ_REL)==0)p.deallocate(a.owner,2,8);}}
static bool boolean(const MenuProfile&p,void*o,const char*name,bool&out){auto v=p.property(o,name);bool ok=false;double n=p.number(&v,&ok);p.variant_destroy(&v);if(!ok||(n!=0&&n!=1))return false;out=n!=0;return true;}
void*AlbumAdapter::object()const{if(!profile.slot||!profile.meta||!profile.cast||!*profile.slot)return nullptr;void*p=*profile.slot;return profile.cast(profile.meta,p)?p:nullptr;}
bool AlbumAdapter::snapshot(void*root,bool storageBrowsing,AlbumContext&out)const{
 auto*content=object();if(!content||!root)return false;AlbumContext next;next.storageBrowsing=storageBrowsing;
 if(!boolean(menu,content,"isBrowsing",next.contentBrowsing))return false;
 auto v=menu.property(content,"path");auto s=menu.string(&v);bool ok=s.size>=0&&s.size<=128&&(s.data||!s.size);if(ok)for(std::ptrdiff_t i=0;i<s.size;i++){auto c=static_cast<char16_t*>(s.data)[i];if(c>127){ok=false;break;}next.path+=char(c);}release(menu,s);menu.variant_destroy(&v);if(!ok||!next.path.valid())return false;
 void*browse=find_named_item(menu,root,u"BrowseView_root");if(browse){void*vm=stock_get_object(stock,browse,"viewModel");bool active=false;if(!vm||!boolean(menu,vm,"browseViewActive",active))return false;if(active){void*item=stock_get_object(stock,vm,"currentItemModel"),*model=stock_get_object(stock,vm,"model");if(!model)return false;auto size=menu.property(model,"listSize");bool numeric=false;double n=menu.number(&size,&numeric);menu.variant_destroy(&size);if(!numeric||n<0||n>10000000)return false;next.viewingPhoto=item&&n>0;}}
 out=static_cast<AlbumContext&&>(next);return true;
}
bool AlbumAdapter::dispatch(const PixelHostAction&a,void*root,bool storageBrowsing)const{
 if(a.kind!=PixelHostActionKind::RefreshAlbumSamePath&&a.kind!=PixelHostActionKind::EnterAlbum&&a.kind!=PixelHostActionKind::UpdateAlbumLoading)return false;
 auto*content=object();if(!content||!root)return false;
 if(a.kind==PixelHostActionKind::UpdateAlbumLoading){if(!profile.update_loading)return false;profile.update_loading(content);return true;}
 AlbumContext current;if(!snapshot(root,storageBrowsing,current)||current.contentBrowsing||current.storageBrowsing||current.viewingPhoto)return false;
 if(!PixelHost::native_album(a.path)||(a.kind==PixelHostActionKind::RefreshAlbumSamePath&&a.path!=current.path))return false;
 auto fn=a.kind==PixelHostActionKind::RefreshAlbumSamePath?profile.path_changed:profile.enter_dir;if(!fn)return false;UiText path;for(char c:a.path)path+=char16_t(static_cast<unsigned char>(c));if(!path.valid())return false;auto value=abi_owned_string(stock,path.data(),path.size());fn(content,value);abi_release_string(stock,value);return true;
}
}
