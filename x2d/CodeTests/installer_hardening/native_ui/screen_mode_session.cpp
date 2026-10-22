#include "screen_mode_session.hpp"
namespace x2dui {
void ScreenModeSession::write(int value){if(haveDesired_&&desired_==value&&pending_)return;haveDesired_=pending_=true;desired_=value;dispatching_=true;if(!api_.forward||!api_.forward(api_.context,value))++failures_;dispatching_=false;}
void ScreenModeSession::releaseOwner(){void*p=owner_;bool remove=attempted_;owner_=nullptr;installed_=attempted_=false;if(p&&remove){dispatching_=true;if(!api_.keys||!api_.keys(api_.context,p,false))++failures_;dispatching_=false;}}
void ScreenModeSession::update(const ScreenModeSnapshot&s){if(closed_||dispatching_||updating_)return;updating_=true;s_=s;
 struct Finish{ScreenModeSession*self;~Finish(){self->updating_=false;if(self->cleanupPending_)self->close();}}finish{this};
 if(pending_&&s.forward==desired_)pending_=false;
 if(policy_.refresh(s.mode)){dispatching_=true;if(!api_.liveview||!api_.liveview(api_.context))++failures_;dispatching_=false;if(closed_)return;}
 bool validBit=s.halfOverrideBit>0&&(s.halfOverrideBit&(s.halfOverrideBit-1))==0;
 if(!policy_.intercepting()||!validBit){releaseOwner();if(closed_)return;auto restore=policy_.release(s.ancestorOverride);if(restore.write)write(restore.value);confirmed_=false;return;}
 if(owner_!=s.focusOwner){releaseOwner();if(closed_)return;owner_=s.focusOwner;if(owner_){attempted_=true;dispatching_=true;installed_=api_.keys&&api_.keys(api_.context,owner_,true);dispatching_=false;if(!installed_)++failures_;if(closed_)return;}}
 auto plan=policy_.forward(s.forward,s.ancestorOverride,s.halfOverrideBit);if(plan.write)write(plan.value);if(closed_)return;confirmed_=s.forward==(s.ancestorOverride|s.halfOverrideBit);
}
bool ScreenModeSession::press(bool half,bool repeat,uint64_t now){return intercepting()&&policy_.pressed(half,repeat,now);}
bool ScreenModeSession::release(bool half){return intercepting()&&policy_.released(half);}
void ScreenModeSession::close(){if(closed_&&!cleanupPending_)return;closed_=cleanupPending_=true;confirmed_=false;if(dispatching_||updating_)return;cleanupPending_=false;releaseOwner();auto p=policy_.release(s_.ancestorOverride);if(p.write)write(p.value);policy_.close();}
}
