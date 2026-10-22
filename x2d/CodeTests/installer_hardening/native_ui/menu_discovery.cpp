#include "menu_discovery.hpp"
#include <cstring>
namespace x2dui {
static_assert(sizeof(AbiArray)==24&&sizeof(AbiVariant)==32,"fixed Qt value ABI");
static void drop(const MenuProfile&p,AbiArray&a,std::ptrdiff_t element){
 if(a.owner){auto*ref=static_cast<int*>(a.owner);int count=__atomic_load_n(ref,__ATOMIC_RELAXED);if(count>=0&&__atomic_sub_fetch(ref,1,__ATOMIC_ACQ_REL)==0)p.deallocate(a.owner,element,8);}
 a.owner=a.data=nullptr;a.size=0;
}
static bool matches(const AbiArray&a,const char*ascii,bool prefix=false){std::size_t n=std::strlen(ascii);if(a.size<0||static_cast<std::size_t>(a.size)<n||(!prefix&&static_cast<std::size_t>(a.size)!=n)||(!a.data&&n))return false;auto*s=static_cast<const char16_t*>(a.data);for(std::size_t i=0;i<n;i++)if(s[i]!=static_cast<unsigned char>(ascii[i]))return false;return true;}
struct Search {const MenuProfile&p;void*page=nullptr;unsigned pages=0,nodes=0;bool invalid=false;};
static void walk(Search&s,void*object,unsigned depth){
 if(!object||s.invalid)return;if(depth>64||++s.nodes>4096){s.invalid=true;return;}
 auto name=s.p.name(object);bool page=matches(name,"SettingsGeneric_root",true);bool ours=matches(name,"X2dNative",true)||matches(name,"X2dExposureEffectOption");drop(s.p,name,2);if(ours)return;
 if(page){auto value=s.p.property(object,"menuName");auto text=s.p.string(&value);bool exposure=matches(text,"exposureMenu");drop(s.p,text,2);s.p.variant_destroy(&value);if(exposure){s.page=object;++s.pages;}}
 auto children=s.p.children(object);if(children.size<0||children.size>4096||(!children.data&&children.size)){drop(s.p,children,8);s.invalid=true;return;}
 auto**items=static_cast<void**>(children.data);for(std::ptrdiff_t i=0;i<children.size;i++)walk(s,items[i],depth+1);drop(s.p,children,8);
}
int find_exposure_menu(const MenuProfile&p,void*root,MenuTarget&out,void*ownedList,double ownedMargin){
 out={};if(!root||!p.cast(p.meta_item,root))return 1;Search search{p};walk(search,root,0);if(search.invalid)return 2;if(search.pages!=1)return search.pages?3:1;
 auto children=p.children(search.page);if(children.size<0||children.size>4096||(!children.data&&children.size)){drop(p,children,8);return 2;}
 void*list=nullptr;int count=0;auto**items=static_cast<void**>(children.data);
 for(std::ptrdiff_t i=0;i<children.size;i++){auto name=p.name(items[i]);if(matches(name,"SettingsGeneric_list")){list=items[i];++count;}drop(p,name,2);}drop(p,children,8);
 if(count!=1||!p.cast(p.meta_itemview,list))return count>1?3:1;
 void*footer=nullptr;void*args[]={&footer,nullptr,nullptr,nullptr};p.itemview(list,1,15,args);if(footer)return 4;
 if(p.bottom_margin(list)!=(list==ownedList?ownedMargin:0))return 5;void*content=p.content(list);if(!content)return 2;
 auto height=p.property(search.page,"heightForItem");bool numeric=false;double row=p.number(&height,&numeric);p.variant_destroy(&height);if(!numeric)row=88;if(!(row>=32&&row<=256))return 6;
 out={search.page,list,content,row};return 0;
}
static bool contains(const MenuProfile&p,void*root,void*object,unsigned depth,unsigned&nodes){
 if(!root||depth>64||++nodes>4096)return false;if(root==object)return true;
 auto children=p.children(root);bool found=false;
 if(children.size>=0&&children.size<=4096&&(children.data||!children.size)){auto**items=static_cast<void**>(children.data);for(std::ptrdiff_t i=0;i<children.size&&!found;i++)found=contains(p,items[i],object,depth+1,nodes);}
 drop(p,children,8);return found;
}
bool menu_contains(const MenuProfile&p,void*root,void*object){unsigned nodes=0;return object&&contains(p,root,object,0,nodes);}

}
