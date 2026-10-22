#pragma once
#include "about_row.hpp"
#include "tree_observer.hpp"
namespace x2dui {
struct AboutAnchorLine {void*item=nullptr;int line=0;};
struct AboutAttachmentProfile {AboutAnchorLine(*right)(const void*)=nullptr;int rightLine=0;};
class AboutAttachment {
 StockProfile stock;MenuProfile menu;SignalProfile signals;FactoryFormatterProfile formatter;BindingProfile binding;AboutAttachmentProfile original;TreeObserver tree;
 struct Row {void*model=nullptr,*label=nullptr;AboutRow*lease=nullptr;SignalWatch*changed=nullptr;};Row rows[2];
 BindingLease animations[8];void*animationItems[8]{};unsigned animationCount=0;void*root=nullptr;bool dirty=true;
 SignalWatch*discovery[96]{};unsigned discoveryCount=0;void*models[32]{};unsigned modelCount=0;
 uint64_t generation=1;bool ticking=false,closePending=false,closing=false;
 struct Candidate {void*model=nullptr,*label=nullptr;};
 static void changed(void*);static void release(void*){}
 bool walk(void*,bool,Candidate*,void**,unsigned&,unsigned&,unsigned depth=0);
 bool rightLabel(void*row,void*&label);bool sameText(void*,const char*,const char16_t*)const;
 void detach();
 void clearDiscovery();bool watchModel(void*);
 void finishTick();
 struct TickScope {AboutAttachment&owner;explicit TickScope(AboutAttachment&o):owner(o){owner.ticking=true;}~TickScope(){owner.finishTick();}};
public:
 AboutAttachment(StockProfile s,MenuProfile m,ViewApi a,SignalProfile g,FactoryFormatterProfile f,BindingProfile b,AboutAttachmentProfile o):stock(s),menu(m),signals(g),formatter(f),binding(b),original(o),tree(m,a,g){}
 ~AboutAttachment(){close();}
 bool tick(void*freshRoot);void close();unsigned active_rows()const{return unsigned(rows[0].lease!=nullptr)+unsigned(rows[1].lease!=nullptr);}
};
}
