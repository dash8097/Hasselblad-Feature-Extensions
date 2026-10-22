#pragma once
#include "protocol_status.hpp"
#include "selection_policies.hpp"
#include <cstddef>
#include <cstdint>
namespace x2dui {
enum class FaceActionKind {DebugMaster,DebugOptions,FaceDetection,FaceTarget,FocusPoint,AfPosUpdated,UpdateAfPos,FaceTapped,ClosePopup,ReadBackend,ReadEyeState};
struct FaceAction {FaceActionKind kind=FaceActionKind::ReadBackend;int64_t value=0;double x=0,y=0;uint64_t id=0;};
class FaceActions {
 FaceAction queue_[32]{};unsigned first_=0,count_=0;bool failed_=false;
public:
 bool push(FaceActionKind,int64_t value=0,double x=0,double y=0,uint64_t id=0)noexcept;
 bool take(FaceAction&)noexcept;
 bool failed()const noexcept{return failed_;}
 void clear()noexcept{first_=count_=0;failed_=false;}
};
struct FaceModeConstants {int off=-1,multi=-1;uint32_t eyeBit=0;};
struct FaceModeSnapshot {bool valid=false,debugMaster=false;uint32_t debugOptions=0;int faceDetection=-1;};
enum class FaceModePhase {Idle,MasterOn,Options,MasterOff,Face};
class FaceModeModel {
 FaceModeConstants c_;FaceModeSnapshot s_;FaceModePhase phase_=FaceModePhase::Idle;
 int wanted_=0,remaining_=0;uint32_t desired_=0,other_=0;bool ownsMaster_=false,applied_=false,error_=false;
 void wait(FaceModePhase);bool options(FaceActions&);bool face(FaceActions&);void fail();
public:
 explicit FaceModeModel(FaceModeConstants c):c_(c){}
 void observe(FaceModeSnapshot s)noexcept{s_=s;}
 int current_mode()const noexcept;
 bool busy()const noexcept{return phase_!=FaceModePhase::Idle;}
 bool error()const noexcept{return error_;}bool owns_master()const noexcept{return ownsMaster_;}
 FaceModePhase phase()const noexcept{return phase_;}
 bool choose(int mode,bool allowed,FaceActions&);
 void detach()noexcept{if(busy())error_=true;phase_=FaceModePhase::Idle;ownsMaster_=false;applied_=false;s_.valid=false;}
 void advance(FaceActions&); // 每100ms一次，先刷新proxy读回；没有隐式setter重试。
 bool take_applied()noexcept{bool a=applied_;applied_=false;return a;}
};
enum class FacePopupKey {Previous,Next,Accept,Abort,Other};
class FacePopupModel {
 int index_=-1,current_=-1,rawMode_=0;bool attached_=false,canChange_=false,busy_=false,multiAllowed_=false,offAllowed_=false,closed_=false;
public:
 void observe(bool attached,bool canChange,bool busy,int current,bool multiAllowed,bool offAllowed,int rawMode=0);
 bool selectable(int index)const noexcept;
 int index()const noexcept{return index_;}bool closed()const noexcept{return closed_;}
 bool key(FacePopupKey,FaceModeModel&,FaceActions&);
 bool press(int index,FaceModeModel&,FaceActions&);
 void applied()noexcept{closed_=true;}
};
using FaceRect=x2d::selection::Rect;
struct FaceCandidate {int id=-1;FaceRect region;};
enum class FaceRoi {Default,Face,EyeLeft,EyeRight,Other};
struct FaceSnapshot {
 bool present=false,faceMode=false,paused=false,idle=false,inSession=false,afRunning=false;
 bool screenBack=false,screenEVF=false,zoomed=false,canMoveTp=false,evfMove=false,touchpadPressed=false;
 bool canShowAF=false,foundFocus=false,selectionActive=false,eyeMode=false;
 int faceModeToken=0; // 原厂face_detection原值，Single/Multi之间变化也必须复位锁定。
 FaceRoi roi=FaceRoi::Other;
 double width=0,height=0,focusX=.5,focusY=.5;
 FaceRect focusRegion,detectedRegion,leftEye,rightEye;
 int detectedId=-1;bool detectedValid=false;
 FaceCandidate candidates[64]{};size_t count=0;
};
struct FacePointState {
 int selectedId=-1,pendingTargetId=-1;bool personLocked=false,requested=false,backendReady=false,eyeBackendReady=false;
 bool ordinaryFallback=false,faceAfActive=false,havePoint=false,pointPending=false;
 double pointX=.5,pointY=.5;FaceRect lockedRegion{0,0,0,0,false};
 bool targetUnconfirmed=false,pointTimedOut=false;unsigned backendAttempts=0;int backendPid=-1,eyeProtocol=0;
};
class FacePointModel {
 FaceSnapshot s_;FacePointState p_;uint64_t now_=0,targetDeadline_=0,pointDeadline_=0,noFaceDeadline_=0,nextProbe_=0,probeId_=0;
 uint64_t nextId_=1;bool attached_=false,dragging_=false;
 bool target_submit_allowed()const noexcept;void refresh_frame();void ordinary(FaceActions&);void evaluate(FaceActions&);void sync_point(FaceActions&);
 void focus(FaceActions&);void face_tapped(FaceActions&,double,double);void update_position(FaceActions&);
public:
 bool update(const FaceSnapshot&,uint64_t now,FaceActions&);
 void tick(uint64_t now,FaceActions&);
 void detach(FaceActions&);
 void backend_reply(uint64_t id,const ProtocolObject&,FaceActions&);
 const FacePointState& state()const noexcept{return p_;}const FaceSnapshot& snapshot()const noexcept{return s_;}
 bool actual_face_valid()const noexcept;bool selector_only()const noexcept;
 bool can_drag()const noexcept;bool owns_evf_touchpad()const noexcept;
 bool lock_nearest(FaceActions&);bool lock_at(double x,double y,FaceActions&);bool unlock(FaceActions&);
 bool move(double x,double y,bool commit,FaceActions&);
 void set_dragging(bool active,FaceActions&);
};
struct EyeInput {
 bool selectionActive=true,eyeMode=false,faceValid=false,personLocked=false,focusConfirmed=false;
 int faceId=-1,lockedFaceId=-1;FaceRect left,right;double width=0,height=0;FaceRoi roi=FaceRoi::Other;
 int lossDelayMs=500;
};
struct EyeState {
 int preferred=-1,active=-1,afEye=-1,afFaceId=-1,preferenceFaceId=-1;
 int64_t afMissingSince=-1;bool pendingTap=false,havePolicy=false;
 double tapX=0,tapY=0;uint64_t deadline=0;
 x2d::selection::EyeState policy{};
};
class EyeModel {
 EyeInput in_;EyeState s_;uint64_t now_=0;
 void calculate();void af_preference();
public:
 void update(EyeInput,uint64_t now);
 void tick(uint64_t now);
 void select_at(int id,double x,double y,uint64_t now);
 bool target_matches()const noexcept;bool valid_eye(int)const noexcept;bool visible_eye()const noexcept;
 const EyeState& state()const noexcept{return s_;}const EyeInput& input()const noexcept{return in_;}
};
class EyeAfModel {
 bool ready_=false,confirmed_=false,error_=false,sent_=false;int wanted_=4,pid_=-1;
 unsigned polls_=0;uint64_t next_=0,readId_=0,serial_=1,sentAt_=0,sampleFloor_=0;int protocol_=1;bool probePending_=false;
 void send(FaceActions&,uint64_t now);
public:
 void update(bool backendReady,int backendPid,int wanted,uint64_t now,FaceActions&,int protocol=1);
 void tick(uint64_t now,FaceActions&);
 void accept(uint64_t id,const ProtocolObject&,uint64_t now=0);
 void detach(FaceActions&);
 bool confirmed()const noexcept{return confirmed_;}bool error()const noexcept{return error_;}
 // 协议2单独请求只读新样本；协议1仍只读取旧回执，不宣称异步新鲜性。
};
}
