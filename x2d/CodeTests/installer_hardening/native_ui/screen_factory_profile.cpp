#include "screen_factory_profile.hpp"
namespace x2dui {namespace {
const char16_t*name(ScreenControlKind k){return k==ScreenControlKind::Display?u"screens_allowed":k==ScreenControlKind::Timeout?u"screen_idle_timeout":nullptr;}
std::ptrdiff_t length(const char16_t*s){std::ptrdiff_t n=0;while(s[n])++n;return n;}
void*formatter(const ScreenFactoryProfile&p){auto&f=p.formatter;if(!f.slot||!*f.slot||!f.cast||!f.meta||!f.call||!f.string_ctor||!f.string_deallocate)return nullptr;return const_cast<void*>(f.cast(f.meta,*f.slot));}
bool text(AbiArray a,char16_t*out){if(a.size<0||a.size>127||(!a.data&&a.size))return false;auto*d=static_cast<const char16_t*>(a.data);for(std::ptrdiff_t i=0;i<a.size;i++){if(!d[i])return false;out[i]=d[i];}out[a.size]=0;return true;}
void release(const ScreenFactoryProfile&p,AbiArray&a){AbiString s{a.owner,static_cast<const char16_t*>(a.data),a.size};abi_release_string(p.formatter,s);}
bool format(const ScreenFactoryProfile&p,const StockProfile&s,void*o,AbiString&field,int64_t value,char16_t*out){if(value<INT32_MIN||value>INT32_MAX)return false;alignas(8)unsigned char v[32];s.variant_int(v,int(value));AbiArray result;void*args[]={&result,&field,v};p.formatter.call(o,0,p.displayMethod,args);bool ok=text(result,out);release(p,result);s.variant_destroy(v);return ok;}
bool translated(const ScreenFactoryProfile&p,void*o,AbiString&field,const char16_t*raw,char16_t*out){auto value=abi_owned_string(p.formatter,raw,length(raw));AbiArray result{};void*args[]={&result,&field,&value};p.formatter.call(o,0,p.translateMethod,args);bool ok=text(result,out);release(p,result);abi_release_string(p.formatter,value);return ok;}
bool fps(const ScreenFactoryProfile&p,bool write,int64_t&value){auto&q=p.proxy;if(!q.slots[0]||!*q.slots[0]||!q.cast||!q.call[0])return false;void*o=const_cast<void*>(q.cast(q.meta[0],*q.slots[0]));if(!o)return false;bool b=value!=0;int status=-1,flags=0;void*args[]={&b,nullptr,&status,&flags};q.call[0](o,write?2:1,p.fpsIndex,args);value=b;return true;}
}
bool screen_factory_snapshot(const ScreenFactoryProfile&p,const StockProfile&s,ScreenControlKind k,bool enabled,ScreenFactorySnapshot&out){
 out.state={};out.state.enabled=enabled;int64_t value=0;
 if(k==ScreenControlKind::AdaptiveFps){if(!fps(p,false,value))return false;out.state.fps=value!=0;return true;}
 auto*fieldName=name(k);void*o=formatter(p);if(!fieldName||!o||!p.list_destroy||!s.variant_int||!s.variant_destroy)return false;
 if(!factory_read(p.proxy,k==ScreenControlKind::Display?FactoryField::ScreensAllowed:FactoryField::ScreenTimeout,value))return false;
 auto field=abi_owned_string(p.formatter,fieldName,length(fieldName));AbiArray list;void*args[]={&list,&field};p.formatter.call(o,0,p.listMethod,args);
 bool ok=list.size>0&&list.size<=64&&list.data;out.state.count=ok?size_t(list.size):0;
 if(ok){auto*items=static_cast<const AbiArray*>(list.data);for(size_t i=0;i<out.state.count;i++){if(!text(items[i],out.rawLabels[i])||!translated(p,o,field,out.rawLabels[i],out.labels[i]))ok=false;out.labelPointers[i]=out.labels[i];}}
 p.list_destroy(&list);
 if(ok)ok=format(p,s,o,field,value,out.current);
 if(ok&&k==ScreenControlKind::Timeout)ok=format(p,s,o,field,0,out.zero);
 abi_release_string(p.formatter,field);if(!ok)return false;
 out.state.labels=out.labelPointers;out.state.current=out.current;out.state.zero=out.zero;out.state.timeoutZero=value==0;
 if(k==ScreenControlKind::Display){out.state.screensAllowed=int(value);out.state.backBit=p.backBit;out.state.evfBit=p.evfBit;if(!factory_read(p.proxy,FactoryField::EvfOnly,value))return false;out.state.evfOnly=value!=0;}
 return true;
}
bool screen_factory_write(const ScreenFactoryProfile&p,const StockProfile&s,ScreenControlKind k,const ScreenWrite&w){
 if(k==ScreenControlKind::AdaptiveFps){int64_t v=w.value;return w.kind==ScreenWriteKind::AdaptiveFps&&(v==0||v==1)&&fps(p,true,v);}
 if(k==ScreenControlKind::Display&&w.kind==ScreenWriteKind::EvfOnly)return factory_write(p.proxy,FactoryField::EvfOnly,w.value);
 if(k==ScreenControlKind::Display&&w.kind==ScreenWriteKind::ScreensAllowed)return w.value==(p.backBit|p.evfBit)&&factory_write(p.proxy,FactoryField::ScreensAllowed,w.value);
 if(k==ScreenControlKind::Timeout&&w.kind==ScreenWriteKind::TimeoutZero)return w.value==0&&factory_write(p.proxy,FactoryField::ScreenTimeout,0);
 if((k!=ScreenControlKind::Display||w.kind!=ScreenWriteKind::DisplayStock)&&(k!=ScreenControlKind::Timeout||w.kind!=ScreenWriteKind::TimeoutStock))return false;
 ScreenFactorySnapshot current;if(!w.label||!screen_factory_snapshot(p,s,k,true,current)||w.sourceIndex<0||size_t(w.sourceIndex)>=current.state.count)return false;
 auto*expected=current.labels[w.sourceIndex];size_t n=0;for(;n<128&&expected[n]&&expected[n]==w.label[n];n++){}if(n>=128||expected[n]!=w.label[n])return false;
 void*o=formatter(p);if(!o||p.systemProxy<0)return false;auto*nm=name(k);auto*raw=current.rawLabels[w.sourceIndex];auto field=abi_owned_string(p.formatter,nm,length(nm));auto value=abi_owned_string(p.formatter,raw,length(raw));unsigned proxy=unsigned(p.systemProxy);void*args[]={nullptr,&proxy,&field,&value};p.formatter.call(o,0,p.setMethod,args);abi_release_string(p.formatter,value);abi_release_string(p.formatter,field);return true;
}
}
