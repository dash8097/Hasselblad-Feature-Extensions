#include "tree_observer.hpp"
#include <cstdlib>
namespace x2dui {
struct TreeObserver::NodeWatch{TreeObserver*owner;void*object;NodeWatch*next;NodeWatch*hashNext;SignalWatch*watches[17]{};unsigned count=0;bool seen=false,moving=false;};
void TreeObserver::motionStarted(void*c){auto*n=static_cast<NodeWatch*>(c);if(!n->moving){n->moving=true;++n->owner->moving;}}
void TreeObserver::motionEnded(void*c){auto*n=static_cast<NodeWatch*>(c);if(n->moving){n->moving=false;if(n->owner->moving)--n->owner->moving;}}
void TreeObserver::nodeDestroyed(void*c){motionEnded(c);auto*n=static_cast<NodeWatch*>(c);n->object=nullptr;n->owner->dirty=n->owner->structureDirty=true;}
void TreeObserver::changed(void*c){static_cast<TreeObserver*>(c)->dirty=true;}
void TreeObserver::structureChanged(void*c){auto*s=static_cast<TreeObserver*>(c);s->dirty=s->structureDirty=true;}
void TreeObserver::destroyed(void*c){auto*s=static_cast<TreeObserver*>(c);s->receiver=nullptr;s->root=nullptr;s->dirty=s->structureDirty=true;}
void TreeObserver::prune(bool all){auto**link=&head;while(*link){auto*n=*link;if(all||!n->seen||!n->object){*link=n->next;motionEnded(n);for(unsigned i=0;i<n->count;i++)signal_unwatch(n->watches[i]);std::free(n);}else link=&n->next;}}
void TreeObserver::clear(){prune(true);}
void TreeObserver::reset(){clear();signal_unwatch(lifetime);lifetime=nullptr;if(receiver)api.destroy(api.context,receiver);receiver=root=nullptr;nodes=moving=0;dirty=structureDirty=true;}
bool TreeObserver::add(NodeWatch*node,const char*signature,bool structural){
 int error=0;auto*w=signal_watch(signals,node->object,receiver,signature,structural?structureChanged:changed,this,release,&error);if(!w)return error==1;
 node->watches[node->count++]=w;++connections;return true;
}
static void drop(const MenuProfile&p,AbiArray&a,std::ptrdiff_t size){if(a.owner&&__atomic_load_n(static_cast<int*>(a.owner),__ATOMIC_RELAXED)>=0&&__atomic_sub_fetch(static_cast<int*>(a.owner),1,__ATOMIC_ACQ_REL)==0)p.deallocate(a.owner,size,8);}
bool TreeObserver::walk(void*node,unsigned depth){
 if(!node||node==receiver)return true;if(depth>64||++nodes>4096)return false;
 auto name=menu.name(node);const char16_t*text=static_cast<const char16_t*>(name.data);bool own=false;
 if(text&&name.size>=9){const char16_t prefix[]=u"X2dNative";own=true;for(unsigned i=0;i<9;i++)if(text[i]!=prefix[i])own=false;}
 if(text&&name.size==23){const char16_t exact[]=u"X2dExposureEffectOption";bool eq=true;for(unsigned i=0;i<23;i++)if(text[i]!=exact[i])eq=false;own|=eq;}drop(menu,name,2);if(own)return true;
 const char*events[]={"childrenChanged()","objectNameChanged(QString)","menuNameChanged()","mainStateChanged()","loaded()","itemChanged()","modelChanged()","contentHeightChanged()","widthChanged()","heightChanged()","visibleChanged()","footerChanged()","bottomMarginChanged()","originYChanged()"};
 // Geometry and visibility invalidate discovery results, not subscriptions.
 // Only child/item replacement requires walking and reconnecting the tree.
 const auto key=(reinterpret_cast<std::uintptr_t>(node)>>4)%1024;
 NodeWatch*entry=buckets[key];while(entry){++lookups;if(entry->object==node)break;entry=entry->hashNext;}
 if(entry){if(entry->seen)return false;entry->seen=true;}
 else{entry=static_cast<NodeWatch*>(std::calloc(1,sizeof(NodeWatch))); if(!entry)return false;entry->owner=this;entry->object=node;entry->next=head;head=entry;entry->seen=true;entry->hashNext=buckets[key];buckets[key]=entry;
  auto*death=signal_watch(signals,node,receiver,"destroyed(QObject*)",nodeDestroyed,entry,release);if(!death)return false;entry->watches[entry->count++]=death;++connections;
  for(unsigned i=0;i<sizeof(events)/sizeof(*events);++i)if(!add(entry,events[i],i==0||i==4||i==5))return false;
  // Flickable delegates churn during a drag/flick. Keep invalidation pending,
  // but let the original animation finish before reconciling that tree.
  const char*motionEvents[]={"movementStarted()","movementEnded()"};
  for(unsigned i=0;i<2;++i){int error=0;auto*w=signal_watch(signals,node,receiver,motionEvents[i],i?motionEnded:motionStarted,entry,release,&error);if(w){entry->watches[entry->count++]=w;++connections;}else if(error!=1)return false;}

 }
 auto children=menu.children(node);bool valid=children.size>=0&&children.size<=4096&&(children.data||!children.size);
 if(valid){auto**items=static_cast<void**>(children.data);for(std::ptrdiff_t i=0;i<children.size&&valid;i++)valid=walk(items[i],depth+1);}drop(menu,children,8);return valid;
}
bool TreeObserver::refresh(void*liveRoot){
 if(!liveRoot){reset();return false;}if(liveRoot!=root||!receiver){reset();root=liveRoot;receiver=api.create(api.context,Kind::Item,root);if(!receiver){root=nullptr;return false;}api.name(api.context,receiver,"X2dNativeTreeObserver");lifetime=signal_watch(signals,receiver,receiver,"destroyed(QObject*)",destroyed,this,release);if(!lifetime){api.destroy(api.context,receiver);receiver=root=nullptr;return false;}}
 if(!dirty||moving)return !invalid;if(!structureDirty){dirty=false;return !invalid;}
 for(auto&bucket:buckets)bucket=nullptr;lookups=0;
 for(auto*n=head;n;n=n->next){n->seen=false;if(n->object){auto key=(reinterpret_cast<std::uintptr_t>(n->object)>>4)%1024;n->hashNext=buckets[key];buckets[key]=n;}}nodes=0;invalid=false;structureDirty=false;++rebuilds;
 if(!walk(root,0)){invalid=true;structureDirty=true;clear();return false;}
 prune(false);dirty=structureDirty;return true;
}
}
