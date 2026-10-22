#include "about_attachment.hpp"
namespace x2dui {
static void drop(const MenuProfile&p,AbiArray&a,unsigned size){if(a.owner&&__atomic_load_n(static_cast<int*>(a.owner),__ATOMIC_RELAXED)>=0&&__atomic_sub_fetch(static_cast<int*>(a.owner),1,__ATOMIC_ACQ_REL)==0)p.deallocate(a.owner,size,8);}
static UiText text(const MenuProfile&p,void*o,const char*name){if(!o)return {};auto v=p.property(o,name);auto s=p.string(&v);UiText out;if(s.size<0||s.size>4096||(!s.data&&s.size))out.invalidate();else out=UiText(static_cast<const char16_t*>(s.data),size_t(s.size));drop(p,s,2);p.variant_destroy(&v);return out;}
bool AboutAttachment::sameText(void*o,const char*n,const char16_t*v)const{return text(menu,o,n)==v;}
static UiText objectName(const MenuProfile&p,void*o){auto a=p.name(o);UiText s;if(a.size<0||a.size>4096||(!a.data&&a.size))s.invalidate();else s=UiText(static_cast<const char16_t*>(a.data),size_t(a.size));drop(p,a,2);return s;}
void AboutAttachment::changed(void*p){static_cast<AboutAttachment*>(p)->dirty=true;}
void AboutAttachment::detach(){for(auto&r:rows){signal_unwatch(r.changed);r.changed=nullptr;if(r.lease)r.lease->close();r={};}for(unsigned i=0;i<animationCount;i++){animations[i].close();animationItems[i]=nullptr;}animationCount=0;}
void AboutAttachment::clearDiscovery(){for(unsigned i=0;i<discoveryCount;i++)signal_unwatch(discovery[i]);discoveryCount=modelCount=0;}
bool AboutAttachment::watchModel(void*model){if(!model)return true;for(unsigned i=0;i<modelCount;i++)if(models[i]==model)return true;if(modelCount==32)return false;models[modelCount++]=model;const char*names[]={"itemNameChanged()","propValueChanged()","destroyed(QObject*)"};for(const char*name:names){int error=0;auto*w=signal_watch(signals,model,root,name,changed,this,release,&error);if(!w&&error!=1)return false;if(w)discovery[discoveryCount++]=w;}return true;}
void AboutAttachment::close(){++generation;if(closing)return;if(ticking){closePending=true;return;}closing=true;clearDiscovery();detach();tree.reset();root=nullptr;dirty=true;closePending=false;closing=false;}
void AboutAttachment::finishTick(){ticking=false;if(closePending)close();}
bool AboutAttachment::rightLabel(void*row,void*&result){
 auto a=menu.children(row);bool ok=a.size>=0&&a.size<=4096&&(a.data||!a.size);unsigned hits=0;
 if(ok)for(std::ptrdiff_t i=0;i<a.size;i++){void*child=static_cast<void**>(a.data)[i];if(!stock_has(stock,child,"text"))continue;void*anchors=stock_get_object(stock,child,"anchors");if(!anchors)continue;auto right=original.right(anchors);if(right.item==row&&right.line==original.rightLine){result=child;++hits;}}
 drop(menu,a,8);return ok&&hits==1;
}
bool AboutAttachment::walk(void*node,bool about,Candidate*candidates,void**animate,unsigned&count,unsigned&visited,unsigned depth){
 if(!node||depth>64||++visited>4096)return false;
 auto name=objectName(menu,node);if(!name.valid())return false;if(name.size()>=9&&!std::memcmp(name.data(),u"X2dNative",18))return true;
 if(stock_has(stock,node,"menuName"))about=sameText(node,"menuName",u"aboutMenu");
 if(about&&(name==u"subDialogLoader"||name==u"specialItem")&&stock_has(stock,node,"animatePopupOpen")){if(count==8)return false;animate[count++]=node;}
 if(about&&name==u"TextDelegate_root"){
  auto*model=stock_get_object(stock,node,"model");if(!watchModel(model))return false;auto field=text(menu,model,"itemName");if(!field.valid())return false;int index=field==u"product_name"?0:field==u"model_number"?1:-1;
  if(index>=0){if(candidates[index].model)return false;void*label=nullptr;if(!rightLabel(node,label))return false;candidates[index]={model,label};}
 }
 auto children=menu.children(node);bool ok=children.size>=0&&children.size<=4096&&(children.data||!children.size);
 if(ok)for(std::ptrdiff_t i=0;i<children.size&&ok;i++)ok=walk(static_cast<void**>(children.data)[i],about,candidates,animate,count,visited,depth+1);drop(menu,children,8);return ok;
}
bool AboutAttachment::tick(void*freshRoot){
 if(ticking||closing)return false;
 if(root!=freshRoot){close();root=freshRoot;}if(!root)return false;
 TickScope scope(*this);const auto epoch=generation;
 bool scan=dirty||tree.pending();if(!tree.refresh(root)){detach();dirty=true;return false;}if(generation!=epoch)return false;if(!scan)return true;
 if(!original.right||original.rightLine!=2){detach();return false;}
 Candidate candidates[2];void*animate[8]{};unsigned count=0,visited=0;
 clearDiscovery();
 // Scope begins false; only a positively identified aboutMenu enables rows.
 if(!walk(root,false,candidates,animate,count,visited)){detach();dirty=true;return false;}if(generation!=epoch)return false;
 bool unchanged=count==animationCount;for(unsigned i=0;i<count&&unchanged;i++)unchanged=animationItems[i]==animate[i];for(unsigned i=0;i<2&&unchanged;i++)unchanged=rows[i].model==candidates[i].model&&rows[i].label==candidates[i].label;
 if(unchanged){dirty=false;return true;}detach();
 for(unsigned i=0;i<count;i++){if(!animations[i].acquire(stock,binding,signals,animate[i],u"animatePopupOpen")||generation!=epoch||!animations[i].number(1)||generation!=epoch){animations[i].close();detach();dirty=true;return false;}animationItems[i]=animate[i];++animationCount;}
 for(unsigned i=0;i<2;i++){auto&c=candidates[i];if(!c.model)continue;auto*r=AboutRow::create(stock,menu,signals,formatter,binding,c.model,c.label,true,true);if(generation!=epoch){if(r)r->close();return false;}if(!r){detach();dirty=true;return false;}rows[i]={c.model,c.label,r,nullptr};int error=0;rows[i].changed=signal_watch(signals,c.model,c.label,"itemNameChanged()",changed,this,release,&error);if(generation!=epoch)return false;if(!rows[i].changed&&error!=1){detach();dirty=true;return false;}}
 dirty=false;return true;
}
}
