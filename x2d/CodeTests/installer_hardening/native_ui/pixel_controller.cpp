#include "pixel_controller.hpp"
#include <algorithm>
#include <limits>
#include <numeric>
namespace x2dui {
namespace {
ProtocolText trim(ProtocolText s){auto ws=[](char c){return c==' '||c=='\n'||c=='\r'||c=='\t';};while(!s.empty()&&ws(s.back()))s.pop_back();size_t n=0;while(n<s.size()&&ws(s[n]))++n;return s.substr(n);}
bool ok(int status){return status==0||status==200;}
ProcessingStage stage_enum(const ProtocolText&s){const char*names[]={"idle","prepare","delay","capture","merge","render","jpeg","jpeg_join","package","convert","raw","publish","restore","cleanup","complete","failed"};for(int i=0;i<16;++i)if(s==names[i])return ProcessingStage(i);return ProcessingStage::Unknown;}
}
int PixelState::save_format()const noexcept{return imageFormat==0||imageFormat==1||imageFormat==3?1:imageFormat==2||imageFormat==4?0:-1;}
PixelController::PixelController(PixelFeatures f):features_(f){s_.resolution=f.pixel400?6:25;}
bool PixelController::issue(Role role,PixelEndpoint endpoint,bool write,const ProtocolText&body){
 auto&p=pending_[role];if(p.active||!body.valid()||(write&&!memory_valid())||nextId_==std::numeric_limits<uint64_t>::max()||now_>std::numeric_limits<uint64_t>::max()-5000)return false;
 Pending candidate={{nextId_++,endpoint,write,body},now_,generation_,true,false,false,write&&(role==ArmWrite||role==QuarterWrite)&&body!="DISARMED\n"};if(!candidate.request.body.valid()){s_.ready=false;s_.lastError=u"界面内存不足，控制请求未发送";return false;}p=static_cast<Pending&&>(candidate);update_sending();return true;
}
void PixelController::update_sending(){s_.sending=false;for(auto&p:pending_)if(p.active&&p.request.write)s_.sending=true;}
bool PixelController::take_request(PixelRequest&out){for(auto&p:pending_)if(p.active&&!p.taken){p.taken=true;out=static_cast<PixelRequest&&>(p.request);return true;}return false;}
bool PixelController::request_dispatchable(uint64_t id)const noexcept{for(const auto&p:pending_)if(p.active&&p.taken&&p.request.id==id)return !p.timedOut&&p.generation==generation_&&(!p.activation||s_.exposureMode==0);return false;}
uint64_t PixelController::request_deadline_ms(uint64_t id)const noexcept{for(const auto&p:pending_)if(p.active&&p.taken&&p.request.id==id)return p.started+5000;return 0;}
void PixelController::discard_unsent(uint64_t id){for(unsigned i=0;i<Count;++i){auto&p=pending_[i];if(!p.active||!p.taken||p.request.id!=id)continue;p.active=false;update_sending();if(p.request.write){s_.lastError=u"控制请求未提交，请重新应用模式";if(i==ArmWrite){s_.applied=s_.preparingMode=s_.preparedMode=false;s_.pendingEpoch=0;}if(i==QuarterWrite)quarterFault_=true;}else if(i==QuarterCheck)quarterFault_=true;return;}}
void PixelController::emit(PixelEventKind kind,const ProtocolText&album){if(eventCount_==events_.size()){s_.lastError=u"界面事件未及时处理";return;}events_[(eventBegin_+eventCount_)%events_.size()]={kind,album};++eventCount_;}
bool PixelController::take_event(PixelEvent&out){if(!eventCount_)return false;out=std::move(events_[eventBegin_]);eventBegin_=(eventBegin_+1)%events_.size();--eventCount_;return true;}
void PixelController::changed_polling(){bool next=s_.polling();if(next&&!wasPolling_){s_.unchangedPolls=0;s_.ready=false;s_.nativeReady=false;}wasPolling_=next;}
void PixelController::set_monitoring(bool v){s_.monitoringRequested=v;changed_polling();}
void PixelController::set_exposure_mode(int value){
 s_.exposureMode=value;
 if(value!=0&&!s_.busy&&(s_.applied||s_.quarterWanted||s_.preparingMode||s_.preparedMode||s_.normalSwitchPending)){
  disarm_impl(true);s_.lastError=u"请切换到 M 档后重新选择模式";
 }
}
void PixelController::tick(uint64_t ms){
 now_=std::max(now_,ms);changed_polling();
 for(unsigned i=0;i<Count;++i){auto&p=pending_[i];if(!p.active||now_-p.started<5000||p.timedOut)continue;p.timedOut=true;
  if(p.request.write){s_.lastError=u"控制写入尚未确认，请读取诊断";continue;} // 不能超时后重发未知写入。
  p.active=false;if(i==Status)s_.ready=false;else if(i==Shutter){s_.nativeReady=false;s_.nativeArm.clear();s_.shutterPending=false;}else if(i==QuarterStatus)s_.quarterReady=false;
  else if(i==QuarterCheck){s_.lastError=u"25MP输出设置未确认";quarterInFlight_.clear();quarterFault_=true;}
 }
 update_sending();if(!s_.polling())return;
 issue(Status,PixelEndpoint::Status,false);issue(Shutter,PixelEndpoint::Shutter,false);issue(QuarterStatus,PixelEndpoint::QuarterStatus,false);publish_quarter();publish_arm();
}
bool PixelController::set_option(const ProtocolText&key,int value){
 if(s_.busy||s_.sending)return false;bool changed=false;
 if(key=="initialDelay"&&s_.resolution!=25&&value>=2&&value<=60){changed=s_.initialDelay!=value;s_.initialDelay=value;}
 else if(key=="keepMode"&&s_.resolution!=25&&(value==0||value==1)){changed=s_.keepMode!=(value==1);s_.keepMode=value==1;}
 else if(key=="pixelResolution"&&(((value==6||value==16)&&features_.pixel400)||(value==25&&features_.quarter))){changed=s_.resolution!=value;s_.resolution=value;}
 else return false;
 if(changed&&s_.applied&&!s_.stackMode){quarterFault_=false;begin_preparation();}return true;
}
ProtocolText PixelController::arm_line()const{
 auto seq=protocol_number(int64_t(s_.acknowledgement)+1);auto prefix=s_.session+" "+seq+" ";
 if(s_.stack_iso_mode())return "RS5 "+prefix+"3 "+protocol_number(s_.stackTargetIso)+" 0 "+protocol_number(s_.stackInitialDelay)+" START END\n";
 if(s_.stackMode)return "RS4 "+prefix+protocol_number(s_.stackMethod)+" "+protocol_number(s_.stackSeconds)+" "+(s_.stackNoise&&s_.stackMethod!=2?"1":"0")+" "+protocol_number(s_.stackInitialDelay)+" START END\n";
 return (s_.resolution==6?"PO3 ":"PO4 ")+prefix+protocol_number(s_.resolution)+" "+protocol_number(s_.resolution==25?0:s_.initialDelay)+" "+(s_.keepMode?"1":"0")+" START END\n";
}
bool PixelController::application_ready()const noexcept{return s_.exposureMode==0&&(s_.quarter_mode()?s_.quarterReady&&s_.nativeReady&&s_.ready&&!s_.busy&&!s_.sending:s_.can_start()&&s_.nativeReady);}
bool PixelController::applied_confirmed()const{return s_.quarter_mode()?s_.applied&&s_.quarterActive&&s_.quarterReady:s_.applied&&s_.preparedMode&&s_.nativeReady&&s_.nativeArm==trim(arm_line());}
bool PixelController::apply_pixel(){if(s_.busy||s_.sending)return false;s_.stackMode=false;return apply_mode();}
bool PixelController::apply_stack(int method,int seconds,bool noise,int delay,int iso){
 (void)noise; // 旧状态兼容：极值模式已移除分组平均，不再传递开启值。
 if(!features_.rawStack||s_.busy||s_.sending||method<0||method>2||seconds<1||seconds>86399||delay<0||delay>60||iso<1||iso>65535)return false;
 if(!s_.applied){s_.stackFrameCount=s_.stackGapMs=0;s_.stackRemaining=seconds;}
 s_.stackSourceIso=s_.stackPlannedFrames=0;s_.stackShutterNumerator=s_.stackShutterDenominator=0;
 s_.stackMode=true;s_.stackMethod=method;s_.stackSeconds=seconds;s_.stackNoise=false;s_.stackInitialDelay=delay;s_.stackTargetIso=iso;s_.keepMode=false;return apply_mode();
}
bool PixelController::apply_mode(){return apply_mode_impl(true);}
bool PixelController::apply_mode_impl(bool explicitAction){
 if(s_.exposureMode!=0){s_.lastError=u"请切换到 M 档后使用";return false;}
 if(explicitAction)quarterFault_=false;
 if(!memory_valid()){s_.ready=false;s_.lastError=u"界面内存不足，控制请求未发送";return false;}
 if(s_.stackMode?!features_.rawStack:s_.quarter_mode()?!features_.quarter:!features_.pixel400)return false;
 if(s_.quarter_mode()){
  if(!application_ready()||s_.save_heif()){s_.lastError=s_.save_heif()?u"25MP请使用JPEG或RAW＋JPEG":u"25MP后台尚未就绪";return false;}
  s_.keepMode=true;s_.quarterWanted=true;s_.normalSwitchPending=false;s_.quarterDisarmed=false;s_.preparingMode=s_.preparedMode=false;s_.pendingEpoch=0;s_.shutterPending=false;
  s_.armIntent="DISARMED\n";s_.lastPublishedArm.clear();publish_arm();publish_quarter();return true;
 }
 if(s_.applied&&s_.armIntent==arm_line()&&(s_.preparingMode||s_.preparedMode))return true;
 if(!s_.can_start()||s_.save_format()<0||!s_.nativeReady||!protocol_session(s_.session)||s_.acknowledgement>=2147483646||s_.modeEpoch>=2147483646){s_.lastError=u"快门后端尚未就绪";return false;}
 begin_preparation();return true;
}
ProtocolText PixelController::quarter_line()const{return s_.quarterWanted?"QN1 25 END\n":"DISARMED\n";}
void PixelController::begin_preparation(){
 if(s_.exposureMode!=0){disarm_impl(false);s_.lastError=u"请切换到 M 档后重新选择模式";return;}
 if(s_.quarter_mode()){apply_mode_impl(false);return;}
 if(s_.quarterWanted||s_.quarterActive||s_.quarterPublished!="DISARMED\n"){s_.quarterWanted=false;s_.normalSwitchPending=true;publish_quarter();return;}
 begin_normal();
}
void PixelController::begin_normal(){
 if(s_.exposureMode!=0){disarm_impl(false);return;}
 if(s_.busy||!protocol_session(s_.session)||s_.acknowledgement>=2147483646||s_.modeEpoch>=2147483646){s_.lastError=u"模式会话已失效，请重新应用";return;}
 s_.pendingEpoch=s_.modeEpoch+1;s_.preparedMode=false;s_.preparingMode=true;s_.stage="prepare";s_.lastError.clear();s_.applied=true;s_.armIntent=arm_line();s_.lastPublishedArm.clear();publish_arm();
}
bool PixelController::disarm_mode(){return disarm_impl(true);}
bool PixelController::disarm_impl(bool explicitAction){
 // 拍摄中不以解除准备冒充取消；RAW 堆栈仅允许 finish_stack 提前结束。
 if(s_.busy)return false;if(explicitAction)quarterFault_=false;s_.quarterWanted=false;s_.normalSwitchPending=false;publish_quarter();s_.shutterPending=false;s_.pendingEpoch=0;s_.preparingMode=s_.preparedMode=false;s_.lastPublishedArm.clear();s_.applied=false;s_.armIntent="DISARMED\n";publish_arm();return true;
}
void PixelController::publish_arm(){if(!s_.initialized||s_.busy||s_.armIntent.empty()||s_.armIntent==s_.lastPublishedArm)return;
 if(issue(ArmWrite,PixelEndpoint::Arm,true,s_.armIntent))s_.lastPublishedArm=s_.armIntent;
}
void PixelController::publish_quarter(){
 if(quarterFault_||pending_[QuarterWrite].active||pending_[QuarterCheck].active||s_.quarterPublished==quarter_line())return;
 // An empty shutter arm confirms only the bridge disarm. The worker may still
 // be restoring its prepared capture; enabling quarter output then would make
 // original_storage() reject its required DISARMED state and retain the lock.
 if(s_.quarterWanted&&(!backendQuiescent_||!s_.quarterReady||!s_.nativeReady||!s_.nativeArm.empty()||!s_.quarterDisarmed))return;
 quarterInFlight_=quarter_line();issue(QuarterWrite,PixelEndpoint::Quarter,true,quarterInFlight_);
}
void PixelController::refresh_disarm(){if(s_.quarterWanted&&s_.nativeReady&&s_.nativeArm.empty())issue(ArmCheck,PixelEndpoint::Arm,false);}
bool PixelController::finish_stack(){if(!s_.busy||finishIssued_||!protocol_session(s_.session))return false;
 if(s_.stage=="delay"){finishIssued_=issue(FinishWrite,PixelEndpoint::Finish,true,"POC1 "+s_.session+" "+protocol_number(s_.acknowledgement)+" CANCEL END\n");return finishIssued_;}
 if(!s_.stackMode||s_.stage!="capture")return false;
 finishIssued_=issue(FinishWrite,PixelEndpoint::Finish,true,"RSF1 "+s_.session+" "+protocol_number(s_.acknowledgement)+" FINISH END\n");return finishIssued_;
}
void PixelController::failed_arm(){s_.lastError=u"模式应用失败";s_.applied=s_.preparingMode=s_.preparedMode=false;s_.pendingEpoch=0;/* 同一次写失败不自动重发。 */}
void PixelController::response(uint64_t id,int status,ProtocolBytes bytes){
 Role role=Count;Pending p;for(unsigned i=0;i<Count;++i)if(pending_[i].active&&pending_[i].taken&&pending_[i].request.id==id){role=Role(i);p=static_cast<Pending&&>(pending_[i]);pending_[i].active=false;break;}if(role==Count)return;update_sending();
 if(p.generation!=generation_&&role!=QuarterWrite&&role!=QuarterCheck){publish_arm();return;}
 bool valid=ok(status)&&bytes.size()<=16384;ProtocolObject value;
 switch(role){
 case Status:if(!valid||!parse_protocol(bytes,value)||!accept_status(value))s_.ready=false;break;
 case Shutter:if(valid&&parse_protocol(bytes,value))accept_shutter(value);else{s_.nativeReady=false;s_.nativeArm.clear();s_.shutterPending=false;}break;
 case QuarterStatus:s_.quarterReady=valid&&trim(ProtocolText(bytes))=="QUARTER_NATIVE_READY";break;
 case ArmWrite:if(!valid)failed_arm();else publish_arm();break;
 case QuarterWrite:if(!valid){s_.lastError=u"25MP输出设置未写入";quarterInFlight_.clear();quarterFault_=true;}else issue(QuarterCheck,PixelEndpoint::Quarter,false);break;
 case QuarterCheck:{
  if(!valid||bytes!=ProtocolBytes(quarterInFlight_)){s_.lastError=u"25MP输出设置未确认";quarterInFlight_.clear();quarterFault_=true;break;}
  s_.quarterPublished=quarterInFlight_;quarterInFlight_.clear();s_.quarterActive=s_.quarterPublished=="QN1 25 END\n";
  if(s_.quarterActive&&s_.quarterWanted&&s_.quarter_mode()){s_.applied=true;s_.stage="idle";s_.lastError.clear();}
  else if(s_.normalSwitchPending&&!s_.quarterWanted){s_.normalSwitchPending=false;begin_normal();}
  publish_quarter();break;}
 case ArmCheck:s_.quarterDisarmed=valid&&s_.quarterWanted&&s_.nativeReady&&s_.nativeArm.empty()&&bytes=="DISARMED\n";if(s_.quarterDisarmed)publish_quarter();break;
 case FinishWrite:if(!valid)s_.lastError=u"提前结束请求未确认，请读取诊断";break;
 case Count:break;
 }changed_polling();
}
bool PixelController::accept_status(const ProtocolObject&v){
 uint64_t ack=0;auto hb=v.get("heartbeat");auto session=v.string("session");
 if(v.integer_or("protocol",3,3,0)!=3||!protocol_session(session)||!v.integer("ack",0,2147483646,ack)||!v.is_boolean("busy")||!v.is_boolean("ready")||!hb||hb->type!=ProtocolValue::Type::Number)return false;
 backendQuiescent_=v.boolean("ready")&&!v.boolean("busy")&&!v.boolean("preparing")&&!v.boolean("prepared");
 bool changed=s_.session!=session;
 if(changed){++generation_;s_.nativeReady=false;s_.nativeArm.clear();s_.nativeWaitPress=-1;s_.quarterDisarmed=false;finishIssued_=false;}
 if(s_.quarterWanted||s_.quarterActive){s_.ready=v.boolean("ready");s_.initialized=true;s_.busy=false;s_.heartbeat=hb->number;s_.session=session;s_.acknowledgement=int(ack);s_.stage="idle";s_.preparingMode=s_.preparedMode=false;if(!memory_valid()){s_.ready=s_.nativeReady=false;return false;}if(statusRevision_!=UINT64_MAX)++statusRevision_;return true;}
 bool complete=v.string("stage")=="complete"&&(changed||s_.completedAcknowledgement!=int(ack));bool wasBusy=s_.busy;
 int frames=int(v.integer_or("frames",0,25));
 if(!v.boolean("stack")&&(v.boolean("busy")||complete)&&(frames==6||frames==16||frames==25))s_.resolution=frames;
 if(v.boolean("stack")&&(v.boolean("busy")||complete||wasBusy)){
  s_.stackMode=true;s_.stackFrameCount=int(v.integer_or("stack_frames",0,2147483647));s_.stackGapMs=int(v.integer_or("stack_gap_ms",0,2147483647));s_.stackRemaining=int(v.integer_or("stack_remaining",0,2147483647));
  if(v.boolean("stack_iso_mode")){s_.stackMethod=3;s_.stackSourceIso=int(v.integer_or("stack_source_iso",1,65535));s_.stackTargetIso=int(v.integer_or("stack_target_iso",1,65535,s_.stackTargetIso));s_.stackPlannedFrames=int(v.integer_or("stack_planned_frames",1,65535));s_.stackShutterNumerator=uint32_t(v.integer_or("stack_shutter_n",1,4294967295ULL));s_.stackShutterDenominator=uint32_t(v.integer_or("stack_shutter_d",1,4294967295ULL));}
 }
 s_.countdownRemaining=int(v.integer_or("remaining",0,60));
 s_.workTotal=int(v.boolean("stack")||frames==25?v.integer_or("work_total",5,6):frames==16?v.integer_or("work_total",55,55):v.integer_or("work_total",16,18));
 s_.workDone=int(v.integer_or("work_done",0,s_.workTotal));s_.renderTiles=int(v.integer_or("render_tiles",0,frames==16?25:9));s_.jpegTiles=int(v.integer_or("jpeg_tiles",0,frames==16?25:9));
 uint64_t rows=0,total=frames==16?34996:17498;bool join=(frames==6||frames==16)&&v.string("stage")=="jpeg_join"&&v.integer_or("jpeg_join_total",total,total)==total&&v.integer("jpeg_join_rows",0,total,rows);
 s_.jpegJoinRows=join?int(rows):0;s_.jpegJoinTotal=join?int(total):0;s_.jpegJoinComplete=join&&v.boolean("jpeg_join_complete")&&rows==total;
 if((v.boolean("busy")||complete)&&!s_.stackMode){if(s_.resolution!=25)s_.initialDelay=int(v.integer_or("delay",2,60,s_.initialDelay));uint64_t keep=0;if(v.integer("keep",0,1,keep))s_.keepMode=keep==1;}
 if(changed){s_.unchangedPolls=0;s_.initialized=false;s_.applied=false;s_.armIntent.clear();s_.lastPublishedArm.clear();s_.pendingEpoch=s_.modeEpoch=0;s_.preparingMode=s_.preparedMode=false;}
 else s_.unchangedPolls=s_.heartbeat==hb->number?std::min(12u,s_.unchangedPolls+1):0;
 s_.session=session;s_.heartbeat=hb->number;s_.acknowledgement=int(ack);if(complete)s_.completedAcknowledgement=int(ack);
 s_.ready=v.boolean("ready");s_.stage=v.string("stage");s_.busy=v.boolean("busy");s_.album=v.string("album");s_.lastError=protocol_utf16(v.string("error"));
 uint64_t epoch=0;if(v.integer("mode_epoch",0,2147483646,epoch)){s_.modeEpoch=int(epoch);
  if(s_.applied&&s_.pendingEpoch>s_.modeEpoch&&!s_.busy){s_.stage="prepare";s_.lastError.clear();}
  if(s_.applied&&s_.pendingEpoch>0&&s_.modeEpoch>=s_.pendingEpoch&&!s_.busy){s_.preparingMode=v.boolean("preparing");s_.preparedMode=v.boolean("prepared");if(s_.stage=="failed"||(!s_.preparingMode&&!s_.preparedMode))disarm_impl(false);}
 }
 if(s_.busy){s_.applied=s_.initialized=true;s_.preparingMode=s_.preparedMode=false;s_.pendingEpoch=0;}
 if(!s_.busy&&(wasBusy||complete)){s_.initialized=true;finishIssued_=false;if(v.string("stage")=="complete"&&s_.keepMode&&!s_.stackMode)begin_preparation();else disarm_impl(false);}
 if(!memory_valid()){s_.ready=false;s_.nativeReady=false;return false;}if(complete)emit(PixelEventKind::Completed,s_.album);if(statusRevision_!=UINT64_MAX)++statusRevision_;return true;
}
void PixelController::accept_shutter(const ProtocolObject&v){
 uint64_t protocol=0,waitCheck=0;auto arm=v.get("arm");bool accepted=v.integer("protocol",1,1,protocol)&&v.is_boolean("ready")&&(!v.boolean("ready")||(arm&&arm->type==ProtocolValue::Type::String))&&(!v.get("pending")||v.is_boolean("pending"))&&(!v.get("wait_press")||v.integer("wait_press",0,4294967295ULL,waitCheck));
 s_.nativeReady=v.integer_or("protocol",1,1)==1&&v.boolean("ready");s_.nativeArm=s_.nativeReady?v.string("arm"):"";
 if(!s_.nativeReady||!s_.nativeArm.empty())s_.quarterDisarmed=false;else refresh_disarm();
 s_.shutterPending=!s_.quarter_mode()&&s_.applied&&!s_.busy&&s_.nativeReady&&s_.nativeArm==trim(arm_line())&&v.boolean("pending");
 uint64_t wait=0;if(s_.nativeReady&&v.integer("wait_press",0,4294967295ULL,wait)){auto previous=s_.nativeWaitPress;s_.nativeWaitPress=int64_t(wait);if(previous>=0&&s_.nativeWaitPress>previous&&s_.applied&&!s_.busy&&!applied_confirmed())emit(PixelEventKind::PreparationWait);}
 auto error=v.get("error");bool truth=error&&((error->type==ProtocolValue::Type::Boolean&&error->boolean)||(error->type==ProtocolValue::Type::String&&!error->string.empty())||(error->type==ProtocolValue::Type::Number&&error->number!=0));
 if(!s_.quarter_mode()&&s_.nativeReady&&truth)s_.lastError=u"快门请求未写入，请重新应用模式";
 if(!s_.quarterWanted&&!s_.quarterActive&&!s_.initialized&&s_.ready){s_.initialized=true;if(!s_.nativeArm.empty()&&s_.nativeArm==trim(arm_line())){s_.applied=true;s_.armIntent=arm_line();s_.lastPublishedArm=s_.armIntent;s_.pendingEpoch=s_.modeEpoch;s_.preparingMode=true;}else if(!s_.busy){s_.applied=false;s_.armIntent="DISARMED\n";publish_arm();}}
 if(accepted&&memory_valid()&&shutterRevision_!=UINT64_MAX)++shutterRevision_;
}
UiText PixelController::save_format_text()const{
 if(!s_.stackMode&&s_.resolution==25)return s_.save_heif()?u"不支持HEIF":u"沿用机内格式";
 if(!s_.stackMode&&s_.resolution!=6)return u"RAW＋JPEG";if(s_.save_format()<0)return u"—";
 if(s_.stackMode)return s_.save_heif()?u"RAW＋HEIF":u"RAW＋JPEG";
 return UiText(u"JPEG")+(s_.save_heif()?u"＋HEIF":u"")+(s_.save_format()==1?u"＋RAW":u"");
}
UiText PixelController::shutter_text()const{if(!s_.stack_iso_mode()||s_.stackFrameCount<=0||!s_.stackShutterNumerator||!s_.stackShutterDenominator)return u"—";
 uint64_t n=uint64_t(s_.stackShutterNumerator)*uint64_t(s_.stackFrameCount),d=s_.stackShutterDenominator,g=std::gcd(n,d);n/=g;d/=g;return protocol_utf16(protocol_number(n)+(d==1?"":"/"+protocol_number(d)))+u" 秒";
}
ProcessingState PixelController::processing_state()const{
 ProcessingState p;p.present=true;p.busy=s_.busy;p.ready=s_.ready;p.preparing=s_.preparingMode;p.stack=s_.stackMode;p.iso=s_.stack_iso_mode();p.joinComplete=s_.jpegJoinComplete;p.stage=stage_enum(s_.stage);p.resolution=s_.resolution;p.captureFrames=s_.resolution==25?1:s_.resolution;
 p.stackRemaining=s_.stackRemaining;p.stackFrames=s_.stackFrameCount;p.plannedFrames=s_.stackPlannedFrames;p.renderTiles=s_.renderTiles;p.jpegTiles=s_.jpegTiles;p.gapMs=s_.stackGapMs;p.outputIso=p.iso&&s_.stackFrameCount>0?double(s_.stackSourceIso)/s_.stackFrameCount:0;p.joinRows=s_.jpegJoinRows;p.joinTotal=s_.jpegJoinTotal;p.workDone=s_.workDone;p.workTotal=s_.workTotal;p.lastError=s_.lastError.c_str();shutterCache_=shutter_text();p.shutter=shutterCache_.c_str();countdownCache_=protocol_utf16(protocol_number(s_.countdownRemaining));p.countdown=countdownCache_.c_str();return p;
}
bool PixelController::memory_valid()const noexcept{return s_.session.valid()&&s_.stage.valid()&&s_.album.valid()&&s_.armIntent.valid()&&s_.lastPublishedArm.valid()&&s_.nativeArm.valid()&&s_.quarterPublished.valid()&&s_.lastError.valid()&&quarterInFlight_.valid();}
}
