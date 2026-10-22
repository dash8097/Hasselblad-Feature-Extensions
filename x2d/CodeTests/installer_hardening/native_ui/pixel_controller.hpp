#pragma once
#include "processing_presentation.hpp"
#include "protocol_status.hpp"
#include <array>
namespace x2dui {
struct PixelFeatures {bool pixel400=true,quarter=true,rawStack=true;};
struct PixelState {
 ProtocolText session,stage="offline",album,armIntent,lastPublishedArm,nativeArm,quarterPublished;
 UiText lastError;
 int acknowledgement=0,completedAcknowledgement=-1,initialDelay=2,resolution=6,imageFormat=-1,exposureMode=-1;
 bool keepMode=false,stackMode=false,stackNoise=false;
 int stackMethod=2,stackSeconds=60,stackInitialDelay=2,stackTargetIso=100,stackSourceIso=0,stackPlannedFrames=0;
 uint32_t stackShutterNumerator=0,stackShutterDenominator=0;
 int stackFrameCount=0,stackGapMs=0,stackRemaining=0;
 bool applied=false,initialized=false,monitoringRequested=true,nativeReady=false,shutterPending=false;
 int64_t nativeWaitPress=-1;
 int modeEpoch=0,pendingEpoch=0;
 bool preparingMode=false,preparedMode=false,quarterReady=false,quarterWanted=false,quarterActive=false,quarterDisarmed=false,normalSwitchPending=false;
 bool busy=false,ready=false,sending=false;
 unsigned unchangedPolls=0;double heartbeat=-1;
 int countdownRemaining=0,workDone=0,workTotal=0,renderTiles=0,jpegJoinRows=0,jpegJoinTotal=0,jpegTiles=0;
 bool jpegJoinComplete=false;
 bool quarter_mode()const noexcept{return !stackMode&&resolution==25;}
 bool stack_iso_mode()const noexcept{return stackMode&&stackMethod==3;}
 int save_format()const noexcept;
 bool save_heif()const noexcept{return imageFormat==3||imageFormat==4;}
 bool polling()const noexcept{return monitoringRequested||applied||busy||sending||quarterWanted||normalSwitchPending||!initialized;}
 bool can_start()const noexcept{return initialized&&ready&&!busy&&!sending&&unchangedPolls<12;}
};
struct PixelRequest {uint64_t id=0;PixelEndpoint endpoint=PixelEndpoint::Status;bool write=false;ProtocolText body;};
enum class PixelEventKind {Completed,PreparationWait};
struct PixelEvent {PixelEventKind kind=PixelEventKind::Completed;ProtocolText album;};
// 单主线程调用；调用方抽取请求后异步完成，不得在析构后回调此对象。
// 不生成捕获请求，不开放任意路径/命令。传输实现必须保持 request.id。
class PixelController {
public:
 explicit PixelController(PixelFeatures={});
 const PixelState& state()const noexcept{return s_;}
 uint64_t status_revision()const noexcept{return statusRevision_;}
 uint64_t shutter_revision()const noexcept{return shutterRevision_;}
 void set_monitoring(bool);
 void set_image_format(int value)noexcept{s_.imageFormat=value;}
 void set_exposure_mode(int value);
 bool set_option(const ProtocolText&,int);
 bool apply_pixel();
 bool apply_stack(int method,int seconds,bool noise,int delay=2,int targetIso=100);
 bool apply_mode();
 bool disarm_mode();
 bool finish_stack();
 void tick(uint64_t monotonicMs);
 bool take_request(PixelRequest&);
 bool request_dispatchable(uint64_t id)const noexcept;
 uint64_t request_deadline_ms(uint64_t id)const noexcept;
 // 仅传输方能证明尚未提交/执行的请求可调用；不是写入成功/失败的假回复。
 void discard_unsent(uint64_t id);
 void response(uint64_t id,int status,ProtocolBytes bytes);
 bool take_event(PixelEvent&);
 ProtocolText arm_line()const;
 bool application_ready()const noexcept;
 bool applied_confirmed()const;
 UiText save_format_text()const;
 UiText shutter_text()const;
 ProcessingState processing_state()const;
 // 提供给协议差分测试/可信原生读回；无任何 I/O。
 bool accept_status(const ProtocolObject&);
private:
 enum Role {Status,Shutter,QuarterStatus,ArmWrite,QuarterWrite,QuarterCheck,ArmCheck,FinishWrite,Count};
 struct Pending {PixelRequest request;uint64_t started=0,generation=0;bool active=false,taken=false,timedOut=false,activation=false;};
 PixelFeatures features_;PixelState s_;std::array<Pending,Count> pending_{};
 uint64_t nextId_=1,now_=0,generation_=0;bool wasPolling_=true;
 uint64_t statusRevision_=0,shutterRevision_=0;
 ProtocolText quarterInFlight_;bool finishIssued_=false,quarterFault_=false,backendQuiescent_=false;
 std::array<PixelEvent,8> events_{};size_t eventBegin_=0,eventCount_=0;
 mutable UiText shutterCache_,countdownCache_;
 bool issue(Role,PixelEndpoint,bool,const ProtocolText& body={});
 void emit(PixelEventKind,const ProtocolText& album={});
 void changed_polling();void update_sending();
 void begin_preparation();void begin_normal();void publish_arm();void publish_quarter();void refresh_disarm();
 void accept_shutter(const ProtocolObject&);ProtocolText quarter_line()const;
 void failed_arm();
 bool apply_mode_impl(bool explicitAction);bool disarm_impl(bool explicitAction);
 bool memory_valid()const noexcept;
};
}
