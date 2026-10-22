#include "binding_lease.hpp"
namespace x2dui {
static AbiString textValue(const StockProfile&p,const char16_t*s){std::ptrdiff_t n=0;if(s)while(s[n])++n;return abi_owned_string(p,s,n);}
void BindingLease::destroyed(void*p){static_cast<BindingLease*>(p)->object=nullptr;}
void BindingLease::clearBinding(AbiBinding&b){AbiBinding zero;binding.assign(&b,zero);}
bool BindingLease::acquire(StockProfile s,BindingProfile p,SignalProfile signals,void*target,const char16_t*name){
 if(active||propertyLive||!target||!name||!s.qml_property_construct||!s.qml_property_write||!s.qml_property_destroy||!p.take||!p.current||!p.install||!p.assign||!p.read||!p.valid||!p.writable||!p.equal)return false;
 stock=s;binding=p;object=target;auto string=textValue(stock,name);stock.qml_property_construct(property,target,string);abi_release_string(stock,string);propertyLive=true;
 if(!binding.valid(property)||!binding.writable(property)){close();return false;}
 watch=signal_watch(signals,target,target,"destroyed(QObject*)",destroyed,this,released);if(!watch){close();return false;}
 before=binding.read(property);expected=binding.read(property);valuesLive=true;original=binding.take(property);active=true;return true;
}
bool BindingLease::write(void*v){if(!alive())return false;auto now=binding.current(property);bool ours=now.pointer==0;clearBinding(now);if(!ours)return false;
 auto value=binding.read(property);ours=binding.equal(&value,&expected);stock.variant_destroy(&value);if(!ours)return false;
 if(!stock.qml_property_write(property,v))return false;if(!object)return false;stock.variant_destroy(&expected);expected=binding.read(property);return true;
}
bool BindingLease::number(double n){if(!alive())return false;alignas(8)unsigned char v[32];stock.variant_double(v,n);bool ok=write(v);stock.variant_destroy(v);return ok;}
bool BindingLease::text(const char16_t*t){if(!alive()||!t)return false;alignas(8)unsigned char v[32];auto s=textValue(stock,t);stock.variant_string(v,s);abi_release_string(stock,s);bool ok=write(v);stock.variant_destroy(v);return ok;}
bool BindingLease::close(){bool restored=!active;
 if(active&&object){auto current=binding.current(property);bool ours=current.pointer==0;clearBinding(current);auto value=binding.read(property);ours=ours&&binding.equal(&value,&expected);stock.variant_destroy(&value);
  if(ours){if(original.pointer){binding.install(&original,property,false);restored=true;}else restored=stock.qml_property_write(property,&before);}
 }
 active=false;if(watch){auto*w=watch;watch=nullptr;signal_unwatch(w);}if(binding.assign)clearBinding(original);if(valuesLive){stock.variant_destroy(&before);stock.variant_destroy(&expected);valuesLive=false;}if(propertyLive){stock.qml_property_destroy(property);propertyLive=false;}object=nullptr;return restored;
}
}
