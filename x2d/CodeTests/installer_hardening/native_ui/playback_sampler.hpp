#pragma once
#include "playback_diagnostics.hpp"
#include "playback_attachment.hpp"
namespace x2dui {
class PlaybackSampler {
 StockProfile stock;MenuProfile menu;AbiViewProfile*abi;SignalProfile signals;PlaybackDiagnostics diagnostics;
 struct EventBinding{PlaybackSampler*self;PlaybackEvent kind;};EventBinding bindings[8];SignalWatch*watches[8]{},*death=nullptr,*rootDeath=nullptr;void*model=nullptr,*receiver=nullptr;
 uint64_t wall=0,lastSample=0;bool sampled=false;
 static void event(void*);static void destroyed(void*);static void rootDestroyed(void*);static void released(void*){}
 void bind(void*,void*);double number(void*,const char*,double fallback=0)const;bool flag(void*,const char*)const;UiText text(void*,const char*)const;
 void*find(void*,void*current,bool preview,unsigned&nodes,unsigned depth=0)const;
public:
 PlaybackSampler(StockProfile s,MenuProfile m,AbiViewProfile*a,SignalProfile g):stock(s),menu(m),abi(a),signals(g){}
 ~PlaybackSampler(){close();}
 void close();void note(PlaybackEvent e,uint64_t at){diagnostics.note(e,at);}
 bool tick(void*freshRoot,const PlaybackAttachment&,bool refreshPending,uint64_t monotonic,uint64_t wallMilliseconds);
 const char*payload()const{return diagnostics.payload();}size_t size()const{return diagnostics.size();}
};
}
