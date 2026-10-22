#pragma once
#include "face_live_profile.hpp"
#include "face_session.hpp"
#include "face_live_route.hpp"
#include "binding_lease.hpp"
#include "screen_key_profile.hpp"
#include "key_mapping.hpp"
namespace x2dui {
// GUI-thread owner. IO shutdown is asynchronous: close(), then drain(), then destroy.
class FaceLiveAttachment final {
 ViewApi api_;AbiViewProfile*abi_;StockProfile stock_;MenuProfile menu_;BindingProfile binding_;SignalProfile signals_;FaceLiveProfile profile_;ScreenKeyProfile keys_;KeyMapProfile map_;
 FaceSession session_;X2dFaceIo*io_=nullptr;FaceDrawPool*drawing_=nullptr;FaceFrame frame_;
 void*blocked_=nullptr;SignalWatch*blockedDeath_=nullptr;
 void*scene_=nullptr,*indicator_=nullptr,*vm_=nullptr,*face_=nullptr,*originalPad_=nullptr,*overlay_=nullptr,*layer_=nullptr,*mouse_=nullptr,*pad_=nullptr;
 struct Life{FaceLiveAttachment*owner=nullptr;void**target=nullptr;};Life life_[9]{};SignalWatch*deaths_[9]{};SignalWatch*mouseSignals_[5]{};SignalWatch*padSignal_=nullptr;
 BindingLease afVisible_,afEnabled_,faceVisible_,faceSelect_,padEnabled_,vmPressed_;
 ScreenKeyReceiver*receiver_=nullptr;ScreenKeyLease keyLease_;uint64_t now_=0,nextSearch_=0;unsigned failures_=0,attempts_=0,foundMask_=0,stage_=0,snapshotStage_=0;int lastRc_=0;
 bool closed_=false,running_=false,deferred_=false,lost_=false,pressed_=false,moved_=false,leased_=false,resetting_=false;double pressX_=0,pressY_=0,dragDistance_=10;
 static bool apply(void*,const FaceAction&);static void blocked_died(void*);void block();static void died(void*);static void release(void*){}
 static void pressed(void*,void**);static void moved(void*,void**);static void released(void*,void**);static void clicked(void*,void**);static void canceled(void*);static void pad_move(void*,void**);static bool physical(void*,const KeyInput&,bool);
 bool discover();bool attach();bool refresh();bool observe_fresh();bool geometry(void*&,double,double,double,double);bool leases(bool);bool mouse_point(void**,double&,double&);void input(int,double,double);void detach();void shutdown();bool named(void*,const char16_t*)const;
public:
 FaceLiveAttachment(ViewApi,AbiViewProfile*,StockProfile,MenuProfile,BindingProfile,SignalProfile,FaceLiveProfile,ScreenKeyProfile,KeyMapProfile,X2dFaceIo*,double dragDistance);
 ~FaceLiveAttachment();
 FaceLiveAttachment(const FaceLiveAttachment&)=delete;FaceLiveAttachment&operator=(const FaceLiveAttachment&)=delete;
 int tick(void*scene,uint64_t now);void close();bool drain();
 bool dispatching()const{return running_||session_.dispatching();}
 bool hooks_ready()const{return !closed_&&!lost_&&indicator_&&vm_&&drawing_&&layer_&&pad_&&receiver_&&io_&&map_.instance&&map_.is_fn&&map_.cast&&map_.unblockable;}
 bool has_source()const{return indicator_;}unsigned failures()const{return failures_;}
 unsigned attempts()const{return attempts_;}unsigned found_mask()const{return foundMask_;}unsigned stage()const{return stage_;}unsigned snapshot_stage()const{return snapshotStage_;}int last_rc()const{return lastRc_;}
 const FaceSession&session()const{return session_;}
};
}
