#pragma once
#include "native_common.hpp"
namespace cn {
// payload 已 fsync 后发布空 gate。独占创建保证不覆盖任何既有请求。
inline void memory_request_publish(const std::string&s){
 require(!exists(s+"/capture.requested")&&!exists(s+"/capture.requested.pending"),"memory-capture-request-reused");
 write_new(s+"/capture.requested.pending","6\n");
 require(read(s+"/capture.requested.pending",3)=="6\n","memory-capture-pending");
 write_new(s+"/capture.requested","");
 require(regular(s+"/capture.requested")&&read(s+"/capture.requested",1).empty(),"memory-capture-request");
}
}
