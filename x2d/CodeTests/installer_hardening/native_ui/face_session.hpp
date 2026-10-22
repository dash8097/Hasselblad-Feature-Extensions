#pragma once
#include "face_model.hpp"
#include "face_io.h"
namespace x2dui {
struct FaceProxyApi {void*context=nullptr;bool(*apply)(void*,const FaceAction&)=nullptr;};
/* GUI主线程拥有；apply是经过版本校验的固定proxy操作，不能启动AF/拍摄。
 * io由外层持有到release成功；这里只取消自己的读请求，不释放外部对象。
 * setter bool仅表示本地派发，不表示原厂接受，模式和点坐标仍靠snapshot读回。 */
class FaceSession {
 struct Pending {uint64_t transport=0,model=0,epoch=0;int endpoint=0;};
 FaceProxyApi proxy_;X2dFaceIo*io_;FaceActions actions_;FacePointModel point_;EyeModel main_,fallback_;EyeAfModel eyeAf_;FaceModeModel mode_;
 Pending pending_[4]{};uint64_t serial_=1,epoch_=1,now_=0,modeTick_=0;unsigned dispatchFailures_=0,readFailures_=0;bool active_=false,draining_=false;
 void drain();void eyes(bool submit=true);void read_replies();void read(const FaceAction&);
public:
 FaceSession(FaceModeConstants c,FaceProxyApi api,X2dFaceIo*io):proxy_(api),io_(io),mode_(c){}
 bool update(const FaceSnapshot&,FaceModeSnapshot,uint64_t now);
 void tick(uint64_t now);
 bool choose_mode(int mode,bool allowed);
 bool lock_nearest();bool lock_at(double x,double y);bool unlock();bool move(double x,double y,bool commit);void dragging(bool);
 void detach();
 const FacePointModel&point()const noexcept{return point_;}const EyeModel&main_eye()const noexcept{return main_;}const EyeModel&fallback_eye()const noexcept{return fallback_;}
 const FaceModeModel&mode()const noexcept{return mode_;}const EyeAfModel&eye_af()const noexcept{return eyeAf_;}
 unsigned dispatch_failures()const noexcept{return dispatchFailures_;}unsigned read_failures()const noexcept{return readFailures_;}
 // Owner必须等dispatching()==false才析构；proxy的同步信号只能detach或排队刷新。
 bool dispatching()const noexcept{return draining_;}
 bool action_overflow()const noexcept{return actions_.failed();}
};
}
