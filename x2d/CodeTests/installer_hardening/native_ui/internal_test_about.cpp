#include "internal_test_about.hpp"
namespace x2dui {
void AboutDiagnostic::attach(uint64_t now){if(phase_!=Disabled)return;phase_=WaitRequest;due_=now+5000;failed_=false;menuOpen_=false;issued_=false;pending_=0;}
void AboutDiagnostic::detach(){phase_=Disabled;issued_=false;pending_=0;menuOpen_=false;}
bool AboutDiagnostic::take(uint64_t now,unsigned aliases,AboutRequest&out){
 if(issued_||now<due_)return false;AboutAction action=AboutAction::None;ProtocolText body;
 if(phase_==WaitRequest){phase_=Reading;action=AboutAction::ReadRequest;}
 else if(phase_==Consuming){action=AboutAction::ConsumeRequest;body="CONSUMED\n";}
 else if(phase_==Open)action=AboutAction::OpenMenu;
 else if(phase_==WaitVerify){phase_=Writing;action=AboutAction::WriteResult;body="ABOUT_DISPLAY_ALIASES="+protocol_number(aliases)+"\n";}
 else if(phase_==Close)action=AboutAction::CloseMenu;
 else return false;
 if(!body.valid()||nextId_==UINT64_MAX){failed_=true;phase_=menuOpen_?Close:Done;return false;}
 pending_=nextId_++;out={pending_,action,static_cast<ProtocolText&&>(body)};issued_=true;return true;
}
void AboutDiagnostic::complete(uint64_t id,bool success,ProtocolBytes bytes,uint64_t now){
 if(!issued_||!id||id!=pending_)return;issued_=false;pending_=0;due_=now;
 if(phase_==Reading){if(success&&bytes=="CHECK_ABOUT\n")phase_=Consuming;else phase_=Done;}
 else if(phase_==Consuming){if(success)phase_=Open;else{failed_=true;phase_=Done;}}
 else if(phase_==Open){if(success){menuOpen_=true;phase_=WaitVerify;due_=now+1200;}else{failed_=true;phase_=Done;}}
 else if(phase_==Writing){if(!success)failed_=true;phase_=Close;}
 else if(phase_==Close){if(!success)failed_=true;menuOpen_=false;phase_=Done;}
}
}
