#include "face_session.hpp"
namespace x2dui {
void FaceSession::read(const FaceAction&a){Pending*p=nullptr;for(auto&v:pending_)if(!v.transport){p=&v;break;}if(!p||!io_||serial_==UINT64_MAX||now_>UINT64_MAX-1000){++readFailures_;return;}
 X2dFaceRequest q{serial_++,now_+1000,a.kind==FaceActionKind::ReadBackend?X2D_FACE_BACKEND:X2D_FACE_EYE};if(!x2d_face_io_submit(io_,&q)){++readFailures_;return;}*p={q.id,a.id,epoch_,q.endpoint};
}
void FaceSession::drain(){if(draining_)return;draining_=true;FaceAction a;unsigned budget=32;while(budget--&&actions_.take(a)){
 if(a.kind==FaceActionKind::ReadBackend||a.kind==FaceActionKind::ReadEyeState){read(a);continue;}
 if(a.kind==FaceActionKind::FaceTapped){main_.select_at(int(a.value),a.x,a.y,now_);continue;}
 /* 即使返回false也消费一次；没有未知setter重发队列。 */
 if(!proxy_.apply||!proxy_.apply(proxy_.context,a))++dispatchFailures_;
}draining_=false;}
void FaceSession::eyes(bool submit){auto&s=point_.snapshot();auto&p=point_.state();EyeInput in;in.selectionActive=s.selectionActive;in.eyeMode=s.eyeMode;in.faceValid=point_.actual_face_valid()&&(!p.personLocked||p.selectedId==s.detectedId);in.personLocked=p.personLocked;in.focusConfirmed=s.foundFocus;in.faceId=s.detectedId;in.lockedFaceId=p.selectedId;in.left=s.leftEye;in.right=s.rightEye;in.width=s.width;in.height=s.height;in.roi=s.roi;main_.update(in,now_);
 in.personLocked=false;in.faceValid=point_.actual_face_valid()&&p.personLocked&&p.selectedId!=s.detectedId;fallback_.update(in,now_);
 int wanted=!s.eyeMode?4:fallback_.visible_eye()&&fallback_.state().active>=0?fallback_.state().active+2:main_.visible_eye()&&main_.state().active>=0?main_.state().active+2:0;
 if(submit)eyeAf_.update(p.eyeBackendReady,p.backendPid,wanted,now_,actions_,p.eyeProtocol);
}
void FaceSession::read_replies(){if(!io_)return;X2dFaceReply reply;for(unsigned n=0;n<4&&x2d_face_io_poll(io_,&reply);n++){Pending found;for(auto&p:pending_)if(p.transport==reply.id){found=p;p={};break;}if(!found.transport||found.epoch!=epoch_||!active_)continue;
 if(reply.status!=200||reply.endpoint!=found.endpoint||reply.length>sizeof(reply.body)){++readFailures_;continue;}ProtocolObject object;if(!parse_protocol(ProtocolBytes(reply.body,reply.length),object)){++readFailures_;continue;}
 if(found.endpoint==X2D_FACE_BACKEND)point_.backend_reply(found.model,object,actions_);else eyeAf_.accept(found.model,object,now_);
 }}
bool FaceSession::update(const FaceSnapshot&s,FaceModeSnapshot mode,uint64_t now){if(draining_||actions_.failed()||now<now_)return false;now_=now;if(!s.present){detach();return true;}if(!point_.update(s,now_,actions_))return false;mode_.observe(mode);active_=true;eyes();drain();return !actions_.failed();}
void FaceSession::tick(uint64_t now){if(draining_||!active_||now<now_)return;now_=now;read_replies();point_.tick(now_,actions_);main_.tick(now_);fallback_.tick(now_);if(now_>=modeTick_){mode_.advance(actions_);modeTick_=now_>UINT64_MAX-100?UINT64_MAX:now_+100;}eyes();eyeAf_.tick(now_,actions_);drain();}
bool FaceSession::choose_mode(int m,bool allowed){if(draining_||!active_)return false;bool ok=mode_.choose(m,allowed,actions_);modeTick_=now_>UINT64_MAX-100?UINT64_MAX:now_+100;drain();return ok;}
bool FaceSession::lock_nearest(){if(draining_||!active_)return false;bool ok=point_.lock_nearest(actions_);eyes(false);drain();eyes();drain();return ok;}
bool FaceSession::lock_at(double x,double y){if(draining_||!active_)return false;bool ok=point_.lock_at(x,y,actions_);eyes(false);drain();eyes();drain();return ok;}
bool FaceSession::unlock(){if(draining_||!active_)return false;bool ok=point_.unlock(actions_);eyes();drain();return ok;}
bool FaceSession::move(double x,double y,bool commit){if(draining_||!active_)return false;bool ok=point_.move(x,y,commit,actions_);eyes();drain();return ok;}
void FaceSession::dragging(bool active){if(active_&&!draining_){point_.set_dragging(active,actions_);eyes();drain();}}
void FaceSession::detach(){if(!active_)return;active_=false;++epoch_;actions_.clear();for(auto&p:pending_)if(p.transport){if(io_)x2d_face_io_cancel(io_,p.transport);p={};}point_.detach(actions_);eyeAf_.detach(actions_);mode_.detach();drain();}
}
