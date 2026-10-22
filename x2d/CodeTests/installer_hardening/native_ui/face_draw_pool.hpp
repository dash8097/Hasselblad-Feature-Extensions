#pragma once
#include "face_view.hpp"
#include "stock_factory.hpp"
namespace x2dui {
struct FaceColorProfile {const void*colorType=nullptr;void(*font_size)(void*,int)=nullptr;};
// Every node is owned native Qt Item/Text/Rectangle; no QML source or painter plugin.
class FaceDrawPool final:private LocalHeap {
 struct Node {FaceDrawPool*owner=nullptr;void*object=nullptr;FaceDrawKind kind=FaceDrawKind::Rectangle;};
 ViewApi api_;StockProfile stock_;FaceColorProfile colors_;void*root_=nullptr;Node nodes_[560]{};unsigned allocated_=0;bool closed_=false,finalized_=false;unsigned busy_=0;int refs_=1;
 FaceDrawPool(ViewApi a,StockProfile s,FaceColorProfile c):api_(a),stock_(s),colors_(c){}
 static void died(void*,Event,double);static void released(void*);static void node_died(void*,Event,double);static void node_released(void*);void drop(){if(!--refs_)delete this;}
 bool color(void*,const char*,uint32_t);bool font(void*,int);void clear();void shutdown();
 struct Mutation{FaceDrawPool*p;explicit Mutation(FaceDrawPool*v):p(v){++p->refs_;++p->busy_;}~Mutation(){--p->busy_;if(p->closed_&&!p->busy_)p->shutdown();p->drop();}};
public:
 static FaceDrawPool*create(ViewApi,StockProfile,FaceColorProfile,void*parent);
 bool update(const FaceFrame&);void close();void*item()const{return root_;}unsigned nodes()const{return allocated_;}
};
}
