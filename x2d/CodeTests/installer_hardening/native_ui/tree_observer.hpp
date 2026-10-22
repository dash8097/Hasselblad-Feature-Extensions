#pragma once
#include "signal_bridge.hpp"
#include "menu_discovery.hpp"
namespace x2dui {
class TreeObserver {
 MenuProfile menu;ViewApi api;SignalProfile signals;void*receiver=nullptr,*root=nullptr;
 struct NodeWatch;NodeWatch*head=nullptr;NodeWatch*buckets[1024]{};unsigned lookups=0;SignalWatch*lifetime=nullptr;unsigned nodes=0,connections=0,moving=0;bool dirty=true,structureDirty=true,invalid=false;unsigned rebuilds=0;
 static void motionStarted(void*);static void motionEnded(void*);static void nodeDestroyed(void*);static void changed(void*);static void structureChanged(void*);static void destroyed(void*);static void release(void*){}
 bool add(NodeWatch*,const char*,bool structural=false);void prune(bool all);bool walk(void*,unsigned);void clear();
public:
 TreeObserver(MenuProfile m,ViewApi a,SignalProfile p):menu(m),api(a),signals(p){}
 ~TreeObserver(){reset();}
 TreeObserver(const TreeObserver&)=delete;
 void reset();bool refresh(void*liveRoot);bool pending()const{return dirty&&!moving;}
 unsigned watchedNodes()const{return nodes;}
 unsigned lookupComparisons()const{return lookups;}
 unsigned subscriptionRebuilds()const{return rebuilds;}
 unsigned subscriptionConnections()const{return connections;}
};
}
