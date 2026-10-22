#include "pixel_host_state.hpp"
#include <QCoreApplication>
#include <QJSEngine>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstdio>
#include <cstdlib>
using namespace x2dui;
static int checks;static void check_at(bool v,int line){++checks;if(!v){std::fprintf(stderr,"PIXEL_HOST_FAIL assertion=%d line=%d\n",checks,line);std::exit(1);}}
#define check(value) check_at((value),__LINE__)
static ProtocolText status(bool busy=false,const char*stage="idle",int epoch=0,bool preparing=false,bool prepared=false,bool stack=false,const char*album=""){
 QJsonObject j{{"protocol",3},{"session","1234abcd"},{"ack",0},{"heartbeat",epoch+1},{"busy",busy},{"ready",true},{"stage",stage},{"mode_epoch",epoch},{"preparing",preparing},{"prepared",prepared},{"stack",stack},{"album",album}};auto b=QJsonDocument(j).toJson(QJsonDocument::Compact);return {b.constData(),size_t(b.size())};
}
static void feed(PixelController&c,const ProtocolText&text){ProtocolObject obj;check(parse_protocol(text,obj));check(c.accept_status(obj));}
static void replies(PixelController&c,const ProtocolText&state,const char*arm=""){
 PixelRequest r;unsigned count=0;while(c.take_request(r)){check(++count<30);ProtocolText body;
  if(r.endpoint==PixelEndpoint::Status)body=state;
  else if(r.endpoint==PixelEndpoint::Shutter){QJsonObject j{{"protocol",1},{"ready",true},{"arm",arm},{"pending",false}};auto b=QJsonDocument(j).toJson(QJsonDocument::Compact);body={b.constData(),size_t(b.size())};}
  else if(r.endpoint==PixelEndpoint::QuarterStatus)body="QUARTER_NATIVE_READY";
  else if(!r.write&&r.endpoint==PixelEndpoint::Quarter)body="DISARMED\n";
  else if(!r.write&&r.endpoint==PixelEndpoint::Arm)body="DISARMED\n";
  c.response(r.id,200,body);
 }
}
static void ready(PixelController&c){c.set_exposure_mode(0);c.set_image_format(0);c.tick(0);replies(c,status());c.tick(1);replies(c,status());check(c.state().initialized&&c.state().can_start()&&c.state().nativeReady);}
int main(int argc,char**argv){QCoreApplication app(argc,argv);QJSEngine js;
 // 生产路径验证用无照片的自造名称，差分原p22两种固定格式。
 const char*paths[]={"/ssd","/ssd/100HASBL","/ssd/999HASBL","/ssd/000HASBL","/ssd/100HASBL/","/ssd/100HASBL/../etc","/ssd//100HASBL","/ssd/100hasbl","/ssd/10HASBL","/ssd/1000HASBL","file:///ssd"};
 for(auto path:paths){js.globalObject().setProperty("value",path);auto expected=js.evaluate("/^\\/ssd(?:\\/[1-9][0-9]{2}HASBL)?$/.test(value)");check(!expected.isError());check(PixelHost::native_album(path)==expected.toBool());}
 {PixelController c;c.set_exposure_mode(0);PixelHost h(c);AlbumContext a;a.path="/ssd/100HASBL";check(h.select(ExtensionMode::Pixel));check(h.apply(100,a));check(h.state().applicationPending&&!h.state().showing);h.observe(2599,a);check(h.state().applicationPending);h.observe(2600,a);check(!h.state().applicationPending&&h.state().showing&&!h.state().applicationError.empty());check(h.dismiss());check(!h.state().showing);}
 {PixelController c;c.set_exposure_mode(0);PixelHost h(c);AlbumContext a;h.select(ExtensionMode::Stack);check(h.apply(0,a));h.select(ExtensionMode::Pixel);h.observe(10,a);check(!h.state().applicationPending&&!h.state().showing);}
 {PixelController c;ready(c);PixelHost h(c);AlbumContext a;a.path="/ssd/100HASBL";check(h.select(ExtensionMode::Pixel));check(h.apply(100,a));check(h.state().preparationPage&&!h.state().showing);check(h.preparation_text()==u"正在准备");
  replies(c,status(false,"prepare",0));feed(c,status(false,"idle",1,false,true));auto arm=c.arm_line();arm.pop_back();c.tick(250);replies(c,status(false,"idle",1,false,true),arm.c_str());h.observe(250,a);check(!h.state().preparationPage&&h.preparation_text().empty());
  feed(c,status(true,"capture",1));h.observe(300,a);check(h.state().showing&&!h.close_processing());
  feed(c,status(false,"complete",1,false,false,false,"101HASBL/A0000001.JPG"));a.viewingPhoto=true;h.observe(400,a);check(!h.state().showing&&h.album_pending());
  PixelHostAction action;while(h.take_action(action)){}h.observe(1000,a);check(h.album_pending()&&!h.take_action(action));a.viewingPhoto=false;h.observe(1200,a);check(!h.album_pending());check(h.take_action(action)&&action.kind==PixelHostActionKind::EnterAlbum&&action.path=="/ssd/101HASBL");check(h.take_action(action)&&action.kind==PixelHostActionKind::UpdateAlbumLoading);check(!h.take_action(action));
  check(!h.completed_album("101HASBL/A0000001.JPG"));check(!h.completed_album("101HASBL/A0000002.3FR"));check(!h.completed_album("../101HASBL/A0000001.JPG"));}
 {PixelController c;ready(c);PixelHost h(c);AlbumContext a;a.path="/ssd/100HASBL";h.queue_album_refresh({});h.storage_available(false);h.observe(0,a);check(h.album_pending());h.storage_available(true);h.observe(1000,a);check(h.album_pending());h.storage_synced();h.observe(1200,a);PixelHostAction action;check(h.take_action(action)&&action.kind==PixelHostActionKind::RefreshAlbumSamePath);check(h.take_action(action)&&action.kind==PixelHostActionKind::UpdateAlbumLoading);}
 {PixelController c;ready(c);PixelHost h(c);AlbumContext a;a.path="/ssd/100HASBL";check(c.set_option("pixelResolution",25));check(h.select(ExtensionMode::Pixel));check(h.apply(1,a));check(c.state().quarterWanted&&!h.state().showing&&!h.state().preparationPage);PixelHostAction action;check(h.take_action(action)&&action.kind==PixelHostActionKind::SetStockSingle);check(h.take_action(action)&&action.kind==PixelHostActionKind::ClosePopup);}
 for(int method=0;method<3;method++){PixelController c;ready(c);PixelHost h(c);AlbumContext a;a.path="/ssd/100HASBL";check(h.stack_options(method,67,true,0,16));check(h.select(ExtensionMode::Stack));check(h.apply(100,a));check(c.state().stackMode&&c.state().stackMethod==method&&c.state().stackTargetIso==16&&c.state().stackInitialDelay==0);check(h.state().preparationPage&&!h.state().showing);check(!h.stack_options(3,60,false,2,100));}
 {PixelController c;ready(c);PixelHost h(c);AlbumContext a;a.path="/ssd";feed(c,status(true,"capture",1));h.observe(0,a);check(!h.apply(10,a)&&!h.stack_options(2,60,false,2,100)&&!h.close_processing());}
 std::printf("PIXEL_HOST_STATE_PASS assertions=%d camera_operations=0\n",checks);
}
