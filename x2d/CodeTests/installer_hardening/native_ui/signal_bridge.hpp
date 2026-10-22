#pragma once
#include "abi_view_api.hpp"
namespace x2dui {
struct AbiMethod {const void*meta;const unsigned*data;};
struct SignalProfile {
 int(*index)(const void*,const char*);AbiMethod(*method)(const void*,int);int(*signal_index)(const AbiMethod&);
 AbiConnection(*connect)(const void*,int,const void*,void*,int);
 bool(*disconnect)(const AbiConnection&);void(*connection_dtor)(AbiConnection*);
 bool(*valid_vtable)(void*,const void*,const void*);void*validation_context;
};
struct SignalWatch;
SignalWatch*signal_watch(const SignalProfile&,void*sender,void*receiver,const char*signature,void(*callback)(void*),void*context,void(*release)(void*),int*error=nullptr);
// Direct connection only: argument pointers are borrowed for this invocation.
SignalWatch*signal_watch_args(const SignalProfile&,void*sender,void*receiver,const char*signature,void(*callback)(void*,void**),void*context,void(*release)(void*),int*error=nullptr);
void signal_unwatch(SignalWatch*);
}
