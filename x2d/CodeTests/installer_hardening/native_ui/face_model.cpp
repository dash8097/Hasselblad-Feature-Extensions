#include "face_model.hpp"
#include <cfloat>
namespace x2dui {
namespace {
bool finite(double x){return x==x&&x<=DBL_MAX&&x>=-DBL_MAX;}
double abs(double x){return x<0?-x:x;}
bool rect(const FaceRect&r){return r.present&&finite(r.x)&&finite(r.y)&&finite(r.width)&&finite(r.height)&&r.width>0&&r.height>0;}
bool present(const FaceSnapshot&s,int id){for(size_t i=0;i<s.count;++i)if(s.candidates[i].id==id)return true;return false;}
int nearest(const FaceSnapshot&s,double x,double y){int result=-1;double best=DBL_MAX;for(size_t i=0;i<s.count;++i){const auto&f=s.candidates[i];const auto&r=f.region;if(f.id<0||f.id>=65535||!rect(r))continue;double dx=r.x>x?r.x-x:x>r.x+r.width?x-r.x-r.width:0,dy=r.y>y?r.y-y:y>r.y+r.height?y-r.y-r.height:0,d=dx*dx+dy*dy;if(d<best||(d==best&&f.id<result)){best=d;result=f.id;}}return result;}
uint64_t deadline(uint64_t now,uint64_t after){return now>UINT64_MAX-after?UINT64_MAX:now+after;}
}
bool FaceActions::push(FaceActionKind kind,int64_t value,double x,double y,uint64_t id)noexcept{if(count_==32){failed_=true;return false;}queue_[(first_+count_)%32]={kind,value,x,y,id};++count_;return true;}
bool FaceActions::take(FaceAction&out)noexcept{if(!count_)return false;out=queue_[first_];first_=(first_+1)%32;--count_;return true;}
int FaceModeModel::current_mode()const noexcept{if(!s_.valid)return -1;return s_.faceDetection==c_.off?2:s_.debugMaster&&(s_.debugOptions&c_.eyeBit)?1:0;}
void FaceModeModel::wait(FaceModePhase p){phase_=p;remaining_=30;}
void FaceModeModel::fail(){phase_=FaceModePhase::Idle;error_=true;}
bool FaceModeModel::options(FaceActions&a){if(phase_==FaceModePhase::MasterOn)other_=s_.debugOptions&~c_.eyeBit;desired_=wanted_==1?s_.debugOptions|c_.eyeBit:s_.debugOptions&~c_.eyeBit;wait(FaceModePhase::Options);if(!a.push(FaceActionKind::DebugOptions,desired_)){fail();return false;}return true;}
bool FaceModeModel::face(FaceActions&a){wait(FaceModePhase::Face);if(!a.push(FaceActionKind::FaceDetection,wanted_==2?c_.off:c_.multi)){fail();return false;}return true;}
bool FaceModeModel::choose(int mode,bool allowed,FaceActions&a){
 if(busy()||!allowed||!s_.valid||mode<0||mode>2||c_.off<0||c_.multi<0||c_.off==c_.multi||!c_.eyeBit||(c_.eyeBit&(c_.eyeBit-1)))return false;
 wanted_=mode;error_=false;applied_=false;
 if(wanted_==1&&!s_.debugMaster){ownsMaster_=true;wait(FaceModePhase::MasterOn);if(!a.push(FaceActionKind::DebugMaster,1)){fail();return false;}return true;}
 return s_.debugMaster?options(a):face(a);
}
void FaceModeModel::advance(FaceActions&a){if(!busy())return;if(--remaining_<0||!s_.valid){fail();return;}
 if(phase_==FaceModePhase::MasterOn&&s_.debugMaster)options(a);
 else if(phase_==FaceModePhase::Options&&s_.debugOptions==desired_){if(wanted_!=1&&ownsMaster_&&(s_.debugOptions&~c_.eyeBit)==other_){wait(FaceModePhase::MasterOff);if(!a.push(FaceActionKind::DebugMaster,0))fail();}else face(a);}
 else if(phase_==FaceModePhase::MasterOff&&!s_.debugMaster){ownsMaster_=false;face(a);}
 else if(phase_==FaceModePhase::Face&&s_.faceDetection==(wanted_==2?c_.off:c_.multi)){if(current_mode()!=wanted_)fail();else{phase_=FaceModePhase::Idle;applied_=true;}}
}
void FacePopupModel::observe(bool attached,bool change,bool busy,int current,bool multi,bool off,int rawMode){bool sync=attached_!=attached||current_!=current||rawMode_!=rawMode;current_=current;rawMode_=rawMode;attached_=attached;canChange_=change;busy_=busy;multiAllowed_=multi;offAllowed_=off;if(!change)closed_=true;if(!attached)index_=-1;else if(sync&&current>=0&&current<3)index_=current;}
bool FacePopupModel::selectable(int i)const noexcept{return !closed_&&attached_&&canChange_&&!busy_&&i>=0&&i<3&&(i==2?offAllowed_:multiAllowed_);}
bool FacePopupModel::press(int i,FaceModeModel&m,FaceActions&a){if(!selectable(i))return false;index_=i;return m.choose(i,true,a);}
bool FacePopupModel::key(FacePopupKey key,FaceModeModel&m,FaceActions&a){if(closed_)return false;if(key==FacePopupKey::Abort){closed_=true;a.push(FaceActionKind::ClosePopup);return true;}if(key==FacePopupKey::Previous){if(index_>0)--index_;return true;}if(key==FacePopupKey::Next){if(index_<2)++index_;return true;}if(key==FacePopupKey::Accept){press(index_,m,a);return true;}return false;}
bool FacePointModel::actual_face_valid()const noexcept{return s_.present&&!s_.paused&&s_.detectedValid&&present(s_,s_.detectedId);}
bool FacePointModel::selector_only()const noexcept{return s_.faceMode&&p_.faceAfActive&&!p_.ordinaryFallback&&!(s_.inSession&&s_.roi==FaceRoi::Default);}
bool FacePointModel::target_submit_allowed()const noexcept{return s_.present&&s_.faceMode&&s_.idle&&!s_.paused&&!s_.afRunning;}
bool FacePointModel::can_drag()const noexcept{return s_.present&&s_.faceMode&&s_.idle&&s_.screenBack&&s_.canMoveTp;}
bool FacePointModel::owns_evf_touchpad()const noexcept{return s_.present&&s_.faceMode&&s_.screenEVF&&!s_.zoomed&&s_.evfMove;}
void FacePointModel::refresh_frame(){p_.lockedRegion={0,0,0,0,false};if(actual_face_valid()&&rect(s_.detectedRegion))p_.lockedRegion=s_.detectedRegion;}
void FacePointModel::sync_point(FaceActions&a){if(!s_.present||dragging_||s_.touchpadPressed)return;if(selector_only()&&p_.havePoint&&!p_.pointPending)return;
 if(p_.pointPending){if(abs(s_.focusX-p_.pointX)>.00015||abs(s_.focusY-p_.pointY)>.00015)return;p_.pointPending=false;pointDeadline_=0;}
 if(!p_.havePoint||abs(s_.focusX-p_.pointX)>.00005||abs(s_.focusY-p_.pointY)>.00005){p_.pointX=s_.focusX;p_.pointY=s_.focusY;p_.havePoint=true;if(!p_.personLocked){p_.selectedId=-1;p_.requested=false;p_.targetUnconfirmed=false;}evaluate(a);}
}
void FacePointModel::ordinary(FaceActions&){if(!s_.faceMode||s_.paused){noFaceDeadline_=0;p_.ordinaryFallback=p_.faceAfActive=false;}
 else if(s_.count){noFaceDeadline_=0;p_.faceAfActive=actual_face_valid();if(p_.faceAfActive){p_.ordinaryFallback=false;p_.pointPending=false;pointDeadline_=0;}}
 else if((p_.faceAfActive||p_.personLocked)&&!p_.ordinaryFallback&&!noFaceDeadline_)noFaceDeadline_=deadline(now_,1000);
}
void FacePointModel::evaluate(FaceActions&a){refresh_frame();if(!s_.present||!s_.faceMode||!p_.havePoint)return;
 if(!p_.personLocked){int id=nearest(s_,p_.pointX*10000,p_.pointY*10000);if(id!=p_.selectedId){p_.selectedId=id;p_.requested=false;p_.targetUnconfirmed=false;}}
 ordinary(a);if(p_.selectedId<0||!present(s_,p_.selectedId))return;
 if(s_.detectedId==p_.selectedId&&p_.pendingTargetId==p_.selectedId){targetDeadline_=0;p_.pendingTargetId=-1;p_.targetUnconfirmed=false;}
 if(p_.requested&&s_.detectedId!=p_.selectedId){if(p_.personLocked&&p_.backendReady)return;if(p_.pendingTargetId==p_.selectedId||p_.targetUnconfirmed)return;p_.requested=false;}
 if(!p_.requested&&target_submit_allowed()){int64_t value=(p_.personLocked&&p_.backendReady?0x40000000LL:0)+p_.selectedId;
  if(a.push(FaceActionKind::FaceTarget,value)){p_.requested=true;p_.pendingTargetId=p_.selectedId;targetDeadline_=deadline(now_,150);}refresh_frame();}
}
void FacePointModel::update_position(FaceActions&a){a.push(FaceActionKind::AfPosUpdated,1,p_.pointX*s_.width,p_.pointY*s_.height);}
void FacePointModel::focus(FaceActions&a){p_.pointPending=true;pointDeadline_=deadline(now_,750);update_position(a);if(!a.push(FaceActionKind::FocusPoint,0,p_.pointX,p_.pointY)){p_.pointPending=false;pointDeadline_=0;}sync_point(a);}
bool FacePointModel::update(const FaceSnapshot&s,uint64_t now,FaceActions&a){
 if(s.count>64||!finite(s.width)||!finite(s.height)||s.width<0||s.height<0||s.width>16384||s.height>16384||!finite(s.focusX)||!finite(s.focusY))return false;
 for(size_t i=0;i<s.count;++i){auto&f=s.candidates[i];if(f.id<0||f.id>=65535||!rect(f.region))return false;for(size_t j=0;j<i;++j)if(s.candidates[j].id==f.id)return false;}
 if(now<now_){targetDeadline_=pointDeadline_=noFaceDeadline_=0;p_.targetUnconfirmed=p_.requested;p_.pointPending=false;}
 bool changed=s_.faceMode!=s.faceMode||s_.faceModeToken!=s.faceModeToken;s_=s;now_=now;
 if(!s.present){detach(a);return true;}if(!attached_){attached_=true;nextProbe_=deadline(now,1000);}
 if(changed){if(p_.backendReady)a.push(FaceActionKind::FaceTarget,0x40010000);targetDeadline_=0;p_.pendingTargetId=-1;p_.selectedId=-1;p_.personLocked=false;p_.requested=false;p_.targetUnconfirmed=false;}
 sync_point(a);evaluate(a);return true;
}
void FacePointModel::tick(uint64_t now,FaceActions&a){if(now<now_)return;now_=now;if(!attached_)return;
 if(!p_.backendReady&&p_.backendAttempts<5&&now>=nextProbe_){probeId_=nextId_++;if(a.push(FaceActionKind::ReadBackend,0,0,0,probeId_)){++p_.backendAttempts;nextProbe_=deadline(now,1000);}}
 if(targetDeadline_&&now>=targetDeadline_){targetDeadline_=0;p_.pendingTargetId=-1;p_.targetUnconfirmed=p_.requested&&s_.detectedId!=p_.selectedId;evaluate(a);}
 if(pointDeadline_&&now>=pointDeadline_){pointDeadline_=0;p_.pointPending=false;p_.pointTimedOut=true;a.push(FaceActionKind::UpdateAfPos);sync_point(a);}
 if(noFaceDeadline_&&now>=noFaceDeadline_){noFaceDeadline_=0;if(!s_.present||!s_.faceMode||s_.count||(!p_.faceAfActive&&!p_.personLocked))return;if(!s_.idle){noFaceDeadline_=deadline(now,1000);return;}p_.ordinaryFallback=true;p_.faceAfActive=false;focus(a);}
}
void FacePointModel::detach(FaceActions&a){if(!attached_)return;if(p_.backendReady)a.push(FaceActionKind::FaceTarget,0x40010000);attached_=false;probeId_=0;targetDeadline_=pointDeadline_=noFaceDeadline_=0;p_.backendReady=p_.eyeBackendReady=false;p_.backendPid=-1;p_.backendAttempts=0;p_.pointPending=false;}
void FacePointModel::backend_reply(uint64_t id,const ProtocolObject&o,FaceActions&a){uint64_t pid=0;if(!attached_||!probeId_||id!=probeId_||o.integer_or("protocol",1,1)!=1||!o.boolean("ready")||!o.integer("pid",1,2147483647,pid))return;probeId_=0;p_.backendReady=true;p_.eyeProtocol=int(o.integer_or("eyeProtocol",1,2));p_.eyeBackendReady=p_.eyeProtocol==1||p_.eyeProtocol==2;p_.backendPid=int(pid);if(p_.personLocked){p_.requested=false;p_.targetUnconfirmed=false;evaluate(a);}else a.push(FaceActionKind::FaceTarget,0x40010000);}
void FacePointModel::face_tapped(FaceActions&a,double x,double y){a.push(FaceActionKind::UpdateAfPos);a.push(FaceActionKind::FaceTapped,p_.selectedId,x,y);refresh_frame();}
bool FacePointModel::lock_nearest(FaceActions&a){if(!s_.present||!s_.faceMode||!s_.idle||p_.pointPending)return false;int target=!p_.personLocked&&present(s_,p_.selectedId)?p_.selectedId:nearest(s_,p_.pointX*10000,p_.pointY*10000);if(target<0)return false;
 p_.selectedId=target;p_.personLocked=true;p_.requested=false;p_.targetUnconfirmed=false;face_tapped(a,p_.pointX*s_.width,p_.pointY*s_.height);evaluate(a);return true;}
bool FacePointModel::lock_at(double x,double y,FaceActions&a){if(!can_drag()||!target_submit_allowed()||s_.width<=0||s_.height<=0||!finite(x)||!finite(y))return false;double px=x/s_.width*10000,py=y/s_.height*10000,best=DBL_MAX;int target=-1;
 for(size_t i=0;i<s_.count;++i){auto&f=s_.candidates[i];auto&r=f.region;if(px<r.x||px>r.x+r.width||py<r.y||py>r.y+r.height)continue;double dx=px-r.x-r.width/2,dy=py-r.y-r.height/2,d=dx*dx+dy*dy;if(d<best){best=d;target=f.id;}}
 if(target<0)return false;p_.pointX=x/s_.width;p_.pointY=y/s_.height;p_.havePoint=true;p_.selectedId=target;p_.personLocked=true;p_.requested=false;p_.targetUnconfirmed=false;face_tapped(a,x,y);evaluate(a);return true;
}
bool FacePointModel::unlock(FaceActions&a){if(!p_.personLocked)return false;if(p_.backendReady)a.push(FaceActionKind::FaceTarget,0x40010000);targetDeadline_=0;p_.pendingTargetId=-1;p_.personLocked=false;p_.selectedId=-1;p_.requested=false;p_.targetUnconfirmed=false;evaluate(a);if(selector_only()||s_.count)return true;focus(a);return true;}
bool FacePointModel::move(double x,double y,bool commit,FaceActions&a){if(!s_.present||!s_.faceMode||!s_.idle||s_.width<=0||s_.height<=0||!finite(x)||!finite(y)||!rect(s_.focusRegion)||s_.focusRegion.width>1||s_.focusRegion.height>1)return false;
 auto&r=s_.focusRegion;p_.pointX=std::max(r.width/2,std::min(1-r.width/2,x/s_.width));p_.pointY=std::max(r.height/2,std::min(1-r.height/2,y/s_.height));p_.havePoint=true;
 if(selector_only()||(!s_.paused&&s_.count)){if(!p_.personLocked)evaluate(a);if(commit)evaluate(a);return true;}update_position(a);evaluate(a);if(commit)focus(a);return true;
}
void FacePointModel::set_dragging(bool active,FaceActions&a){bool was=dragging_;dragging_=active;if(was&&!active){sync_point(a);evaluate(a);}}
bool EyeModel::target_matches()const noexcept{return in_.faceId>=0&&(!in_.personLocked||in_.faceId==in_.lockedFaceId);}
bool EyeModel::valid_eye(int i)const noexcept{auto&r=i==0?in_.left:in_.right;return in_.faceId>=0&&rect(r)&&r.x<1&&r.y<1&&r.x+r.width>0&&r.y+r.height>0;}
bool EyeModel::visible_eye()const noexcept{return in_.selectionActive&&in_.eyeMode&&in_.faceValid&&target_matches()&&(in_.personLocked?s_.active>=0:valid_eye(0)||valid_eye(1));}
void EyeModel::calculate(){if(!in_.eyeMode||!in_.faceValid||!target_matches()){s_.active=-1;return;}bool left=valid_eye(0),right=valid_eye(1);
 if(in_.personLocked){if(s_.preferenceFaceId!=in_.faceId){s_.preferenceFaceId=in_.faceId;s_.preferred=-1;s_.havePolicy=false;}if(s_.pendingTap||s_.preferred<0){int chosen=x2d::selection::nearest_eye(in_.left,in_.right,s_.tapX,s_.tapY,in_.width,in_.height);if(chosen<0){s_.active=-1;return;}s_.preferred=chosen;s_.pendingTap=false;s_.havePolicy=false;}}
 else{s_.pendingTap=false;if(!s_.havePolicy||s_.policy.face_id!=in_.faceId){s_.preferred=left?0:1;s_.havePolicy=false;}}
 if(!s_.havePolicy){s_.policy=x2d::selection::create_eye(s_.preferred,in_.faceId);s_.havePolicy=true;}auto result=x2d::selection::eye_step(s_.policy,s_.preferred,in_.faceId,left,right,double(now_),in_.lossDelayMs);s_.policy=result.state;s_.active=result.eye;
}
void EyeModel::af_preference(){if(!in_.eyeMode||(in_.personLocked&&in_.lockedFaceId!=s_.afFaceId)||(in_.faceId>=0&&s_.afFaceId>=0&&in_.faceId!=s_.afFaceId)){s_.afEye=-1;s_.afMissingSince=-1;s_.afFaceId=-1;}
 if(in_.eyeMode&&in_.faceValid&&target_matches()&&s_.active>=0){s_.afEye=s_.active;s_.afFaceId=in_.faceId;s_.afMissingSince=-1;}else if(s_.afEye>=0){if(s_.afMissingSince<0||now_<uint64_t(s_.afMissingSince))s_.afMissingSince=int64_t(now_);if(now_-uint64_t(s_.afMissingSince)>=uint64_t(in_.lossDelayMs))s_.afEye=-1;}
}
void EyeModel::update(EyeInput in,uint64_t now){if(now>uint64_t(INT64_MAX)||in.lossDelayMs<0||in.lossDelayMs>60000||!finite(in.width)||!finite(in.height)||in.width<0||in.height<0){s_.active=-1;s_.deadline=0;return;}
 if(in.personLocked!=in_.personLocked){s_.havePolicy=false;s_.preferred=-1;s_.pendingTap=false;}in_=in;now_=now;s_.deadline=0;
 if(!in.selectionActive){s_.active=s_.afEye=s_.afFaceId=-1;s_.afMissingSince=-1;s_.havePolicy=false;return;}calculate();af_preference();uint64_t due=UINT64_MAX;
 if(in.eyeMode&&in.faceValid&&target_matches()&&s_.havePolicy){if(s_.policy.missing_since>=0&&valid_eye(1-s_.policy.chosen))due=std::min(due,deadline(uint64_t(s_.policy.missing_since),in.lossDelayMs));if(s_.policy.preferred_since>=0)due=std::min(due,deadline(uint64_t(s_.policy.preferred_since),in.lossDelayMs));}
 if(s_.afEye>=0&&s_.afMissingSince>=0)due=std::min(due,deadline(uint64_t(s_.afMissingSince),in.lossDelayMs));if(due!=UINT64_MAX)s_.deadline=due>now?due:deadline(now,1);
}
void EyeModel::tick(uint64_t now){if(s_.deadline&&now>=s_.deadline)update(in_,now);}
void EyeModel::select_at(int id,double x,double y,uint64_t now){if(id<0||id>=65535||!finite(x)||!finite(y))return;s_.preferenceFaceId=id;s_.tapX=x;s_.tapY=y;s_.pendingTap=true;s_.preferred=-1;s_.havePolicy=false;s_.active=-1;update(in_,now);}
void EyeAfModel::send(FaceActions&a,uint64_t now){if(!ready_||sent_||wanted_<0||wanted_>4)return;if(a.push(FaceActionKind::FaceTarget,0x40020000LL+wanted_)){sent_=true;sentAt_=now;next_=deadline(now,500);}}
void EyeAfModel::update(bool ready,int pid,int wanted,uint64_t now,FaceActions&a,int protocol){if(wanted<0||wanted>4||pid<1||(protocol!=1&&protocol!=2))ready=false;bool change=ready_!=ready||pid_!=pid||wanted_!=wanted||protocol_!=protocol;ready_=ready;pid_=pid;wanted_=wanted;protocol_=protocol;if(change){confirmed_=error_=sent_=probePending_=false;polls_=0;readId_=0;}send(a,now);}
void EyeAfModel::tick(uint64_t now,FaceActions&a){if(!ready_||confirmed_||error_||!sent_||now<next_)return;
 if(protocol_==2&&!probePending_){if(polls_>=3){error_=true;return;}if(a.push(FaceActionKind::FaceTarget,0x40020005)){++polls_;readId_=0;sampleFloor_=now;probePending_=true;next_=deadline(now,1);}return;}
 if(protocol_==1&&polls_>=3){error_=true;return;}if(serial_==UINT64_MAX){error_=true;return;}readId_=serial_++;if(a.push(FaceActionKind::ReadEyeState,wanted_,0,0,readId_)){if(protocol_==1)++polls_;probePending_=false;next_=deadline(now,500);}}
void EyeAfModel::accept(uint64_t id,const ProtocolObject&o,uint64_t now){if(!ready_||!readId_||id!=readId_||o.integer_or("protocol",1,2)!=uint64_t(protocol_)||o.integer_or("pid",1,2147483647)!=uint64_t(pid_))return;
 if(protocol_==2){uint64_t command=0,sample=0,issued=0;if(!o.integer("command",0,4,command)||command!=uint64_t(wanted_)||!o.integer("sampled_ms",sampleFloor_,now,sample)||!o.integer("command_ms",sentAt_,sample,issued))return;}
 uint64_t req=0,actual=0,result=1;if(!o.integer("requested",0,4,req)||!o.integer("actual",0,3,actual)||!o.integer("result",0,2147483647,result))return;confirmed_=result==0&&actual==req&&(wanted_==4||req==uint64_t(wanted_));if(confirmed_)error_=false;else if(polls_>=3)error_=true;
}
void EyeAfModel::detach(FaceActions&a){if(ready_)a.push(FaceActionKind::FaceTarget,0x40020004);ready_=false;readId_=0;}
}
