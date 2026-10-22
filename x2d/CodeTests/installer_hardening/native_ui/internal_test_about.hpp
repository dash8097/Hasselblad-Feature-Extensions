#pragma once
#include "about_display.hpp"
namespace x2dui {
// 仅内部UI自动化测试；不得链接入生产候选。
enum class AboutAction {None,ReadRequest,ConsumeRequest,OpenMenu,WriteResult,CloseMenu};
struct AboutRequest {uint64_t id=0;AboutAction action=AboutAction::None;ProtocolText body;};
class AboutDiagnostic {
 enum Phase {Disabled,WaitRequest,Reading,Consuming,Open,WaitVerify,Writing,Close,Done};
 Phase phase_=Disabled;bool issued_=false;uint64_t due_=0,nextId_=1,pending_=0;
 bool failed_=false,menuOpen_=false;
public:
 // 每个nativeMenu实例只开始一次；对象销毁由detach通知，不借用失效指针。
 void attach(uint64_t monotonicMs);
 void detach();
 bool take(uint64_t monotonicMs,unsigned aliasCount,AboutRequest&);
 void complete(uint64_t id,bool success,ProtocolBytes bytes,uint64_t monotonicMs);
 bool failed()const{return failed_;}
 bool finished()const{return phase_==Done||phase_==Disabled;}
};
}
