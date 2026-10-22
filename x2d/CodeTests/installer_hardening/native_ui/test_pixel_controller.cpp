#include "pixel_controller.hpp"
#include <QCoreApplication>
#include <QFile>
#include <QJSEngine>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace x2dui;
static long allocationBudget=-1;static unsigned allocationHits=0;
extern "C" void*__real_realloc(void*,size_t);
extern "C" void*__wrap_realloc(void*p,size_t n){if(allocationBudget==0){++allocationHits;return nullptr;}if(allocationBudget>0)--allocationBudget;return __real_realloc(p,n);}
static int checks=0;
static void check(bool b,const char*name){++checks;if(!b){std::fprintf(stderr,"PIXEL_FAIL %s at %d\n",name,checks);std::exit(1);}}
static ProtocolObject object(const std::string&s){ProtocolObject o;check(parse_protocol(s,o),"parse");return o;}
static std::string status(const char*extra="",const char*session="0123abcd",int ack=0,int heartbeat=1){return "{\"protocol\":3,\"session\":\""+std::string(session)+"\",\"ack\":"+std::to_string(ack)+",\"busy\":false,\"ready\":true,\"heartbeat\":"+std::to_string(heartbeat)+",\"stage\":\"idle\""+extra+"}";}
static std::vector<PixelRequest> take(PixelController&c){std::vector<PixelRequest> r;PixelRequest p;while(c.take_request(p))r.push_back(p);return r;}
static void drain(PixelController&c,const std::string&shutter="",const std::string&workerStatus=status()){for(int i=0;i<8;++i){auto requests=take(c);if(requests.empty())break;for(auto&p:requests){std::string body;
 if(p.endpoint==PixelEndpoint::Status)body=workerStatus;else if(p.endpoint==PixelEndpoint::Shutter)body="{\"protocol\":1,\"ready\":true,\"arm\":\""+shutter+"\",\"pending\":false,\"wait_press\":0}";
 else if(p.endpoint==PixelEndpoint::QuarterStatus)body="QUARTER_NATIVE_READY\n";else if(p.endpoint==PixelEndpoint::Quarter&&!p.write)body=c.state().quarterWanted?"QN1 25 END\n":"DISARMED\n";else if(p.endpoint==PixelEndpoint::Arm&&!p.write)body="DISARMED\n";
 c.response(p.id,0,body);
 }}}
static void ready(PixelController&c){c.set_exposure_mode(0);c.set_image_format(0);c.tick(0);drain(c);c.tick(250);drain(c);check(c.state().can_start()&&c.state().nativeReady,"ready handshake");}
static QString body(const QString&s,const QString&marker){int p=s.indexOf(marker);check(p>=0,"oracle marker");p=s.indexOf('{',p);auto begin=p+1;int depth=1;while(++p<s.size()&&depth){if(s[p]=='{')++depth;else if(s[p]=='}')--depth;}return s.mid(begin,p-begin-1);}
int main(int argc,char**argv){QCoreApplication app(argc,argv);check(argc==2,"source argument");QFile f(QString::fromLocal8Bit(argv[1]));check(f.open(QIODevice::ReadOnly),"source read");auto source=QString::fromUtf8(f.readAll());
 ProtocolObject o;for(auto invalid:{"", "[]", "{\"x\":1,\"x\":2}","{\"x\":01}","{\"x\":NaN}","{\"x\":1e9999}","{\"x\":\"\\ud800\"}","{\"x\":1}x","{\"x\":true,}"})check(!parse_protocol(invalid,o),"malformed rejected");
 check(!parse_protocol(std::string(17000,' '),o),"size cap");check(!parse_protocol("{\"x\":\"\xc0\xaf\"}",o),"overlong utf8");
 check(parse_protocol("{\"unicode\":\"\\ud83d\\ude00\",\"unknown\":[1,true,{}],\"n\":4294967295}",o),"unicode and unknown");check(protocol_utf16(o.string("unicode")).size()==2,"unicode pair");
 check(o.integer_or("n",0,4294967295ULL)==4294967295ULL,"uint32 ceiling");
 check(!parse_protocol("{\"x\":{\"x\":{\"x\":{\"x\":{\"x\":1}}}}}",o),"depth cap");
 check(!parse_protocol("{\"x\":\""+std::string(4097,'x')+"\"}",o),"text cap no truncation");
 check(!parse_protocol("{\"a\":1,\"\\u0061\":2}",o),"decoded duplicate key");
 std::string many="{";for(int i=0;i<65;++i){if(i)many+=",";many+='\"'+std::to_string(i)+"\":0";}many+='}';check(!parse_protocol(many,o),"field cap");
 check(parse_protocol("{\"x\":1.25e2,\"y\":-0.25e1,\"z\":1e-9999}",o)&&o.integer_or("x",0,1000)==125&&o.get("y")->number==-2.5&&o.get("z")->number==0,"locale independent numbers");
 unsigned failurePositions=0;for(int budget=0;budget<40;++budget){ProtocolObject limited;allocationHits=0;allocationBudget=budget;bool accepted=parse_protocol("{\"protocol\":3,\"session\":\"0123abcd\",\"ack\":0,\"busy\":false,\"ready\":true,\"heartbeat\":1}",limited);allocationBudget=-1;if(allocationHits){++failurePositions;check(!accepted,"injected parse allocation failure rejected");}
  if(accepted)check(limited.string("session")=="0123abcd"&&limited.count==6,"oom never truncated accepted object");else check(limited.count==0,"oom preserves old output");
 }
 check(failurePositions==7,"seven actual parser allocation failure positions");
 {ProtocolText limited("old");allocationBudget=0;limited+=std::string(200,'a');allocationBudget=-1;check(!limited.valid()&&limited.size()==3,"sticky allocation failure");}
 PixelController c;ready(c);check(c.arm_line()=="PO3 0123abcd 1 6 2 0 START END\n","PO3 exact");check(c.apply_pixel(),"apply six");auto req=take(c);check(req.size()==1&&req[0].endpoint==PixelEndpoint::Arm&&req[0].body==c.arm_line(),"only arm write");c.response(req[0].id,0,"");
 check(c.accept_status(object(status(",\"mode_epoch\":1,\"prepared\":true,\"preparing\":false"))),"prepare confirmation");
 c.tick(500);drain(c,"PO3 0123abcd 1 6 2 0 START END");check(c.applied_confirmed(),"native plus worker ready");
 check(c.disarm_mode(),"disarm prepare");req=take(c);check(req.size()==1&&req[0].body=="DISARMED\n","cancel does not capture");c.response(req[0].id,0,"");
 for(int method=0;method<3;++method){check(c.apply_stack(method,60,true,0,50),"stack all modes");auto expected="RS4 0123abcd 1 "+std::to_string(method)+" 60 0 0 START END\n";check(c.arm_line()==expected,"stack wire exact");drain(c);check(c.disarm_mode(),"stack disarm");drain(c);}
 check(!c.apply_stack(3,60,false)&&!c.apply_stack(0,0,false)&&!c.apply_stack(0,10,false,61)&&!c.apply_stack(3,10,false,2,65536),"stack bounds");
 check(c.accept_status(object("{\"protocol\":3,\"session\":\"0123abcd\",\"ack\":1,\"busy\":true,\"ready\":false,\"heartbeat\":2,\"stage\":\"capture\",\"stack\":true}")),"capture status");
 check(!c.disarm_mode()&&!c.set_option("pixelResolution",16),"busy cannot fake cancel");check(c.finish_stack()&&!c.finish_stack(),"finish once");req=take(c);check(req.size()==1&&req[0].endpoint==PixelEndpoint::Finish&&req[0].body=="RSF1 0123abcd 1 FINISH END\n","finish exact");c.response(req[0].id,500,"");check(!c.finish_stack(),"failed finish not replayed");
 PixelController timeout;ready(timeout);check(timeout.apply_pixel(),"timeout arm");req=take(timeout);timeout.tick(6000);check(timeout.state().sending&&!timeout.state().lastError.empty(),"write timeout retained");for(auto&p:take(timeout))check(!p.write,"no timed out write replay");timeout.response(req[0].id,0,"");check(!timeout.state().sending,"late reply releases");
 PixelController held;ready(held);check(held.apply_pixel(),"queued arm");req=take(held);check(req.size()==1&&held.request_dispatchable(req[0].id),"queued current request dispatchable");check(held.request_deadline_ms(req[0].id)==5250&&held.request_deadline_ms(999999)==0,"original issue deadline exposed");held.tick(5249);check(held.request_dispatchable(req[0].id),"deadline minus one dispatchable");held.tick(5250);check(!held.request_dispatchable(req[0].id)&&held.request_deadline_ms(req[0].id)==5250,"exact deadline expired without extension");held.tick(6000);check(!held.request_dispatchable(req[0].id),"queued timed out arm not dispatchable");held.discard_unsent(req[0].id);check(!held.state().sending&&!held.state().applied&&!held.state().lastError.empty(),"proven unsent discarded without fake success");
 PixelController epoch;ready(epoch);check(epoch.apply_pixel(),"epoch arm");req=take(epoch);check(epoch.accept_status(object(status("","89abcdef"))),"new daemon session");check(!epoch.request_dispatchable(req[0].id),"old queued arm session not dispatchable");epoch.discard_unsent(req[0].id);check(!epoch.state().sending,"old unsent arm cleared");
 PixelController stale;stale.tick(0);auto old=take(stale);stale.tick(6000);auto fresh=take(stale);for(auto&p:old)if(p.endpoint==PixelEndpoint::Status)stale.response(p.id,0,status());check(stale.state().session.empty(),"stale read ignored");for(auto&p:fresh)if(p.endpoint==PixelEndpoint::Status)stale.response(p.id,0,status());check(stale.state().session=="0123abcd","fresh read accepted");
 // Real-device regression: prepared six-frame -> quarter must not enable
 // quarter while the old runner's original-storage verification is pending.
 {PixelController switching;ready(switching);check(switching.apply_pixel(),"transition prepare six");
  auto prepared=status(",\"mode_epoch\":1,\"prepared\":true,\"preparing\":false");
  drain(switching,"PO3 0123abcd 1 6 2 0 START END",prepared);check(switching.accept_status(object(prepared)),"transition worker prepared");
  check(switching.set_option("pixelResolution",25),"transition select quarter");drain(switching,"",prepared);
  switching.tick(1000);drain(switching,"",prepared);check(switching.state().quarterWanted&&!switching.state().quarterActive,"empty arm cannot bypass prepared runner");
  auto restoring="{\"protocol\":3,\"session\":\"0123abcd\",\"ack\":0,\"busy\":false,\"ready\":false,\"heartbeat\":3,\"stage\":\"idle\",\"prepared\":false,\"preparing\":false}";
  switching.tick(1250);drain(switching,"",restoring);check(!switching.state().quarterActive,"retained recovery lock blocks quarter");
  switching.tick(1500);drain(switching,"",status());switching.tick(1750);drain(switching);
  check(switching.state().quarterActive,"quarter starts only after unprepared ready receipt");}
 {PixelController cancel;ready(cancel);check(cancel.accept_status(object("{\"protocol\":3,\"session\":\"0123abcd\",\"ack\":7,\"busy\":true,\"ready\":true,\"heartbeat\":2,\"stage\":\"delay\",\"frames\":16}")),"countdown observed");check(cancel.finish_stack()&&!cancel.finish_stack(),"cancel requested once");auto commands=take(cancel);check(commands.size()==1&&commands[0].endpoint==PixelEndpoint::Finish&&commands[0].body=="POC1 0123abcd 7 CANCEL END\n","countdown cancel bound to exact task");}
 PixelController q;ready(q);check(q.set_option("pixelResolution",25),"quarter selection");q.set_image_format(3);check(!q.apply_pixel(),"quarter HEIF reject");q.set_image_format(0);check(q.apply_pixel(),"quarter apply");drain(q);q.tick(500);drain(q);check(q.applied_confirmed(),"quarter readback");check(q.set_option("pixelResolution",16),"quarter to sixteen");drain(q);check(q.arm_line()=="PO4 0123abcd 1 16 2 1 START END\n","sixteen wire");check(q.state().preparingMode&&!q.state().quarterActive,"quarter disarm before prepare");
 PixelController disabled({false,false,false});ready(disabled);check(!disabled.apply_pixel()&&!disabled.apply_stack(0,5,false)&&!disabled.set_option("pixelResolution",6),"feature gates");
 PixelController ages;ready(ages);for(int i=0;i<13;++i)check(ages.accept_status(object(status())),"heartbeat sample");check(!ages.state().can_start(),"unchanged heartbeat cap");
 for(int failure=0;failure<3;++failure){PixelController blocked;blocked.tick(0);for(auto&p:take(blocked)){if(p.endpoint==PixelEndpoint::Quarter&&p.write)blocked.response(p.id,failure==0?500:0,"");else blocked.response(p.id,0,p.endpoint==PixelEndpoint::Status?status():"{}");}
  for(auto&p:take(blocked))if(p.endpoint==PixelEndpoint::Quarter&&failure==1)blocked.response(p.id,0,"MISMATCH\n");blocked.tick(6000);
  for(auto&p:take(blocked))check(!(p.endpoint==PixelEndpoint::Quarter&&p.write),"quarter failed write/readback/timeout no automatic replay");
  check(blocked.accept_status(object("{\"protocol\":3,\"session\":\"0123abcd\",\"ack\":1,\"busy\":false,\"ready\":true,\"heartbeat\":2,\"stage\":\"complete\",\"frames\":6}")),"automatic completion after quarter fault");
  for(auto&p:take(blocked))check(!(p.endpoint==PixelEndpoint::Quarter&&p.write),"completion must not reset quarter fault");
  check(blocked.disarm_mode(),"explicit new disarm");bool retry=false;for(auto&p:take(blocked))if(p.endpoint==PixelEndpoint::Quarter&&p.write)retry=true;check(retry,"explicit action can retry quarter configuration");
 }
 PixelController closed;ready(closed);check(closed.set_option("pixelResolution",25)&&closed.apply_pixel(),"quarter apply before closing UI");closed.set_monitoring(false);drain(closed);check(closed.state().polling(),"pending quarter keeps polling");closed.tick(500);drain(closed);check(closed.applied_confirmed(),"closed UI quarter confirmation");
 PixelController events;ready(events);const auto complete="{\"protocol\":3,\"session\":\"0123abcd\",\"ack\":1,\"busy\":false,\"ready\":true,\"heartbeat\":2,\"stage\":\"complete\",\"frames\":6,\"album\":\"synthetic\"}";check(events.accept_status(object(complete))&&events.accept_status(object(complete)),"repeated completion");PixelEvent event;check(events.take_event(event)&&event.kind==PixelEventKind::Completed&&event.album=="synthetic"&&!events.take_event(event),"completion dedup");
 PixelController keep;ready(keep);check(keep.set_option("keepMode",1)&&keep.apply_pixel(),"keep prepare");drain(keep);check(keep.accept_status(object(complete)),"keep complete");req=take(keep);check(req.size()==1&&req[0].endpoint==PixelEndpoint::Arm&&req[0].body=="PO3 0123abcd 2 6 2 1 START END\n","keep reprepare next ack only arm");
 PixelController wait;ready(wait);check(wait.apply_pixel(),"wait prepare");drain(wait);wait.tick(500);for(auto&p:take(wait)){if(p.endpoint==PixelEndpoint::Shutter)wait.response(p.id,0,"{\"protocol\":1,\"ready\":true,\"arm\":\"PO3 0123abcd 1 6 2 0 START END\",\"pending\":true,\"wait_press\":1}");else wait.response(p.id,0,p.endpoint==PixelEndpoint::Status?status():"QUARTER_NATIVE_READY");}check(wait.take_event(event)&&event.kind==PixelEventKind::PreparationWait&&wait.state().shutterPending,"physical shutter waiting notice");
 wait.set_monitoring(false);check(wait.state().polling(),"applied mode polls on hidden UI");
 {PixelController revision;check(revision.status_revision()==0&&revision.shutter_revision()==0,"initial observation revisions");check(revision.accept_status(object(status()))&&revision.status_revision()==1,"accepted status advances");check(!revision.accept_status(object("{}"))&&revision.status_revision()==1,"invalid status does not advance");revision.response(999,200,status());check(revision.status_revision()==1,"unknown id does not advance");}
 {PixelController revision;auto parsed=object(status());allocationHits=0;allocationBudget=0;bool accepted=revision.accept_status(parsed);allocationBudget=-1;check(allocationHits>0&&!accepted&&revision.status_revision()==0,"status state OOM does not advance");}
 {PixelController revision;ready(revision);uint64_t now=500;for(auto bad:{"{}","{\"protocol\":2,\"ready\":true,\"arm\":\"\"}","{\"protocol\":1,\"ready\":true,\"arm\":1}","{\"protocol\":1,\"ready\":true,\"arm\":\"\",\"pending\":1}","{\"protocol\":1,\"ready\":true,\"arm\":\"\",\"wait_press\":-1}"}){auto before=revision.shutter_revision();revision.tick(now);now+=250;bool found=false;for(auto&p:take(revision))if(p.endpoint==PixelEndpoint::Shutter){found=true;revision.response(p.id,200,bad);}else revision.response(p.id,200,p.endpoint==PixelEndpoint::Status?status():"DISARMED\n");check(found&&revision.shutter_revision()==before,"bad shutter never becomes fresh observation");}auto before=revision.shutter_revision();revision.tick(now);for(auto&p:take(revision))if(p.endpoint==PixelEndpoint::Shutter)revision.response(p.id,200,"{\"protocol\":1,\"ready\":true,\"arm\":\"\",\"pending\":false,\"wait_press\":0}");check(revision.shutter_revision()==before+1,"valid shutter advances");}
 {PixelController revision;ready(revision);revision.tick(500);auto old=take(revision);auto sr=revision.shutter_revision();check(revision.accept_status(object(status("","89abcdef"))),"new generation for revision");auto st=revision.status_revision();for(auto&p:old)revision.response(p.id,200,p.endpoint==PixelEndpoint::Status?status():"{\"protocol\":1,\"ready\":true,\"arm\":\"\"}");check(revision.shutter_revision()==sr&&revision.status_revision()==st,"old generation replies cannot advance revisions");}
 // User policy: all extension entries require a fresh M-mode observation.
 for(int variant=0;variant<7;++variant)for(int mode:{-1,1,2,3,4,255}){
  if(variant==3)continue; // retired ISO stacking is refused in every mode
  auto select=[&](PixelController&v){return variant<4?v.apply_stack(variant,7,false,0,16):(v.set_option("pixelResolution",variant==4?6:variant==5?16:25)&&v.apply_pixel());};
  PixelController gated;ready(gated);gated.set_exposure_mode(mode);check(!select(gated),"non-M-entry-refused");for(auto&r:take(gated))check(!r.write||r.body=="DISARMED\n","non-M-no-activation");
  gated.set_exposure_mode(0);check(select(gated),"M-explicit-reentry-accepted");
  auto queued=take(gated);gated.set_exposure_mode(mode);
  for(auto&r:queued)if(r.write&&r.body!="DISARMED\n"){check(!gated.request_dispatchable(r.id),"mode-changed-queued-activation-refused");gated.discard_unsent(r.id);}else gated.response(r.id,0,"");
  check(!gated.state().applied&&!gated.state().preparingMode&&!gated.state().quarterWanted,"leaving-M-removes-preparation");
  gated.tick(500);for(auto&r:take(gated))check(!r.write||r.body=="DISARMED\n","only-disarm-after-leaving-M");
 }
 {PixelController inFlight;ready(inFlight);check(inFlight.set_option("keepMode",1)&&inFlight.apply_pixel(),"M-keep-before-active");drain(inFlight);
  check(inFlight.accept_status(object("{\"protocol\":3,\"session\":\"0123abcd\",\"ack\":1,\"busy\":true,\"ready\":false,\"heartbeat\":3,\"stage\":\"capture\",\"frames\":6}")),"busy-fixture");
  inFlight.set_exposure_mode(1);check(inFlight.state().busy,"mode-change-does-not-fake-capture-cancellation");check(inFlight.accept_status(object(complete)),"completion-after-mode-change");
  for(auto&r:take(inFlight))check(!r.write||r.body=="DISARMED\n","keep-never-rearms-outside-M");check(!inFlight.state().preparingMode,"no-reprepare-outside-M");}
 // 原 p21 acceptStatus 标量/进度差分，不执行文件通道。选 busy=true 规避异步环境。
 QJSEngine js;auto eval=[&](QString s){auto v=js.evaluate(s);check(!v.isError(),"oracle eval");return v;};
 eval("function acceptStatus(value){"+body(source,"function acceptStatus(value)")+"};function disarmMode(){};function beginPreparation(){};function completed(){};");
 const char*stages[]={"capture","merge","jpeg_join","render","failed"};
 for(int mode=0;mode<5;++mode)for(int stage=0;stage<5;++stage)for(int sample=0;sample<7;++sample){
  PixelController n;int frames=mode==1?16:mode==2?25:6;bool stack=mode>=3;int total=stack||frames==25?6:frames==16?55:18;
  QJsonObject v{{"protocol",3},{"session","0123abcd"},{"ack",2},{"busy",true},{"ready",false},{"heartbeat",12},{"stage",stages[stage]},{"stack",stack},{"frames",frames},{"delay",sample+1},{"keep",sample%3},{"stack_frames",sample*3},{"stack_gap_ms",sample*11},{"stack_remaining",10-sample},{"stack_iso_mode",mode==4},{"stack_source_iso",400},{"stack_target_iso",50},{"stack_planned_frames",8},{"stack_shutter_n",1},{"stack_shutter_d",100},{"remaining",sample*12},{"work_total",sample==0?0:total},{"work_done",sample*4},{"render_tiles",sample*5},{"jpeg_tiles",sample*5},{"jpeg_join_total",frames==16?34996:17498},{"jpeg_join_rows",sample==6?40000:sample*100},{"jpeg_join_complete",sample==5},{"mode_epoch",3}};
  eval("var session='',acknowledgement=0,completedAcknowledgement=-1,initialDelay=2,pixelResolution=6,keepMode=false,stackMode=false,stackMethod=2,stackTargetIso=100,stackSourceIso=0,stackPlannedFrames=0,stackShutterNumerator=0,stackShutterDenominator=0,stackFrameCount=0,stackGapMs=0,stackRemaining=0,quarterWanted=false,quarterActive=false,busy=false,ready=false,heartbeat=-1,stage='offline',applied=false,initialized=false,sending=false,unchangedPolls=0,armIntent='',lastPublishedArm='',modeEpoch=0,pendingEpoch=0,preparingMode=false,preparedMode=false;");
  auto json=QJsonDocument(v).toJson(QJsonDocument::Compact);check(n.accept_status(object(json.toStdString())),"native status");check(eval("acceptStatus("+QString::fromUtf8(json)+")").toBool(),"oracle accepted");auto&s=n.state();
  const std::pair<const char*,int> pairs[]={{"pixelResolution",s.resolution},{"initialDelay",s.initialDelay},{"stackMethod",s.stackMethod},{"stackFrameCount",s.stackFrameCount},{"stackGapMs",s.stackGapMs},{"stackRemaining",s.stackRemaining},{"stackSourceIso",s.stackSourceIso},{"stackTargetIso",s.stackTargetIso},{"stackPlannedFrames",s.stackPlannedFrames},{"countdownRemaining",s.countdownRemaining},{"workDone",s.workDone},{"workTotal",s.workTotal},{"renderTiles",s.renderTiles},{"jpegTiles",s.jpegTiles},{"jpegJoinRows",s.jpegJoinRows},{"jpegJoinTotal",s.jpegJoinTotal},{"modeEpoch",s.modeEpoch}};
  for(auto&p:pairs)check(eval(p.first).toInt()==p.second,p.first);
  check(eval("stackMode").toBool()==s.stackMode&&eval("keepMode").toBool()==s.keepMode&&eval("jpegJoinComplete").toBool()==s.jpegJoinComplete,"boolean differential");
  auto ps=n.processing_state();check(ps.resolution==frames&&ps.stack==stack,"presentation bridge");
 }
 std::printf("PIXEL_CONTROLLER_PASS assertions=%d differential_scenarios=175 actual_parse_allocation_failure_positions=%u camera_requests=0\n",checks,failurePositions);
}
