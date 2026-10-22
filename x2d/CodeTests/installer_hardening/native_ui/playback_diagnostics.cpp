#include "playback_diagnostics.hpp"
#include <cmath>
#include <cstdio>
namespace x2dui {
namespace {
const char*metricNames[]={"screenWidth","screenHeight","contentWidth","contentHeight","contentX","contentY","previewWidth","previewHeight","previewStatus","fullStatus","fullScale","decodeWidth","decodeHeight","decodeX","decodeY","effectiveX","effectiveY","regionRequests","regionReadyMs","readyTiles","failedTiles","fastDecodeMs","paintedWidth","paintedHeight"};
const char*flagNames[]={"fullPresent","fullVisible","fastPath","fullFrameHardware"};
const char*eventNames[]={"browse-closed","exit-zoom","exposing-changed","screen-evf-changed","screen-back-changed","screen-hdmi-changed","current-item-changed","zoomed-changed","album-refresh"};
static_assert(sizeof(metricNames)/sizeof(*metricNames)==PlaybackDisplay::MetricCount,"metric contract");
struct Json {
 char*p;size_t size,at=0;bool valid=true;
 void raw(const char*s){while(*s){if(at+1>=size){valid=false;return;}p[at++]=*s++;}p[at]=0;}
 void integer(uint64_t n){char b[32];std::snprintf(b,sizeof b,"%llu",(unsigned long long)n);raw(b);}
 void number(double n){if(!std::isfinite(n)){raw("null");return;}if(n==0){raw("0");return;}char b[48];int count=std::snprintf(b,sizeof b,"%.17g",n);if(count<0||count>=int(sizeof b)){valid=false;return;}for(char*c=b;*c;c++)if(*c==',')*c='.';raw(b);}
 void boolean(bool n){raw(n?"true":"false");}
 void key(const char*k){raw("\"");raw(k);raw("\":");}
 void text(const UiText&s){if(!s.valid()||s.size()>256){valid=false;return;}raw("\"");for(char16_t c:s){char b[7];std::snprintf(b,sizeof b,"\\u%04x",unsigned(c));raw(b);}raw("\"");}
 void display(const PlaybackDisplay&d){raw("{");key("previewState");text(d.previewState);for(int i=0;i<PlaybackDisplay::MetricCount;i++){raw(",");key(metricNames[i]);number(d.metrics[i]);}for(int i=0;i<PlaybackDisplay::FlagCount;i++){raw(",");key(flagNames[i]);boolean(d.flags[i]);}raw("}");}
};
bool same(const PlaybackDisplay&a,const PlaybackDisplay&b){
 if(a.previewState!=b.previewState)return false;
 for(int i=0;i<PlaybackDisplay::MetricCount;i++)if(a.metrics[i]!=b.metrics[i]&&(std::isfinite(a.metrics[i])||std::isfinite(b.metrics[i])))return false;
 for(int i=0;i<PlaybackDisplay::FlagCount;i++)if(a.flags[i]!=b.flags[i])return false;return true;
}
}
bool PlaybackDiagnostics::note(PlaybackEvent reason,uint64_t at){
 if(!enabled_||samples_>=1800||int(reason)<0||int(reason)>=9)return false;
 if(eventCount_==16){eventStart_=(eventStart_+1)%16;--eventCount_;}events_[(eventStart_+eventCount_)%16]={at,reason};++eventCount_;return true;
}
bool PlaybackDiagnostics::sample(const PlaybackSnapshot&s,uint64_t at){
 length_=0;payload_[0]=0;if(!enabled_||samples_>=1800||!s.display.previewState.valid()||s.display.previewState.size()>256)return false;
 if(!hasLast_||!same(last_,s.display)){
  PlaybackDisplay next=s.display;if(!next.previewState.valid())return false;
  PlaybackDisplay entry=next;if(!entry.previewState.valid())return false;
  unsigned slot=(changeStart_+changeCount_)%8;changes_[slot].display=static_cast<PlaybackDisplay&&>(entry);changes_[slot].at=at;
  if(changeCount_==8)changeStart_=(changeStart_+1)%8;else ++changeCount_;
  last_=static_cast<PlaybackDisplay&&>(next);hasLast_=true;
 }
 ++samples_;Json j{payload_,sizeof payload_};j.raw("{\"version\":3,\"at\":");j.integer(at);j.raw(",\"sequence\":");j.integer(samples_);
 const char*keys[]={"browseActive","zoomed","refreshPending","currentPresent","previewPresent"};bool flags[]={s.browseActive,s.zoomed,s.refreshPending,s.currentPresent,s.previewPresent};
 for(unsigned i=0;i<5;i++){j.raw(",");j.key(keys[i]);j.boolean(flags[i]);}
 j.raw(",\"modelWidth\":");j.number(s.modelWidth);j.raw(",\"modelHeight\":");j.number(s.modelHeight);j.raw(",\"screenBack\":");j.boolean(s.screenBack);j.raw(",\"screenEVF\":");j.boolean(s.screenEVF);j.raw(",\"exposing\":");j.boolean(s.exposing);j.raw(",\"display\":");j.display(s.display);
 j.raw(",\"changes\":[");for(unsigned i=0;i<changeCount_;i++){auto&c=changes_[(changeStart_+i)%8];if(i)j.raw(",");j.raw("{\"at\":");j.integer(c.at);j.raw(",\"state\":");j.display(c.display);j.raw("}");}
 j.raw("],\"events\":[");for(unsigned i=0;i<eventCount_;i++){auto&e=events_[(eventStart_+i)%16];if(i)j.raw(",");j.raw("{\"at\":");j.integer(e.at);j.raw(",\"reason\":\"");j.raw(eventNames[int(e.reason)]);j.raw("\"}");}j.raw("]}");
 if(!j.valid){payload_[0]=0;return false;}length_=j.at;return true;
}
}
