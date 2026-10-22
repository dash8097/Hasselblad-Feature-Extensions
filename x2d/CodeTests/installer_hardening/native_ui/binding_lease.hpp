#pragma once
#include "stock_factory.hpp"
#include "signal_bridge.hpp"
namespace x2dui {
struct AbiBinding {uintptr_t pointer=0;~AbiBinding(){}};
struct BindingProfile {
 AbiBinding(*take)(const void*);AbiBinding(*current)(const void*);
 void(*install)(void*,const void*,bool);void*(*assign)(void*,const AbiBinding&);
 AbiVariant(*read)(const void*);bool(*valid)(const void*);bool(*writable)(const void*);
 bool(*equal)(const AbiVariant*,const AbiVariant*);
};
// Main-thread lease of an existing GUI property, not a camera proxy setter.
// A foreign binding/value installed while leased is never overwritten.
class BindingLease final {
 StockProfile stock{};BindingProfile binding{};void*object=nullptr;SignalWatch*watch=nullptr;
 alignas(8)unsigned char property[8]{};AbiBinding original;AbiVariant before{},expected{};
 bool propertyLive=false,valuesLive=false,active=false;
 static void destroyed(void*);static void released(void*){}
 void clearBinding(AbiBinding&);bool write(void*variant);
public:
 BindingLease()=default;~BindingLease(){close();}
 BindingLease(const BindingLease&)=delete;BindingLease&operator=(const BindingLease&)=delete;
 bool acquire(StockProfile,BindingProfile,SignalProfile,void*object,const char16_t*name);
 bool number(double);bool text(const char16_t*);
 bool close();bool alive()const{return active&&object;}
};
}
