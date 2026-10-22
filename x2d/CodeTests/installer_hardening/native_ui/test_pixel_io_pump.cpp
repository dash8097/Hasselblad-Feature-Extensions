#include "pixel_io_pump.hpp"
#include <vector>
#include <string>
#include <cstring>
#include <cstdio>
#include <cstdlib>
using namespace x2dui;
// This executable tests the adapter, not files or the worker implementation.
extern "C" int x2d_pixel_io_submit(X2dPixelIo*,const X2dPixelIoRequest*){std::abort();}
extern "C" int x2d_pixel_io_poll(X2dPixelIo*,X2dPixelIoReply*){std::abort();}
extern "C" int x2d_pixel_io_cancel(X2dPixelIo*,uint64_t){std::abort();}
static int checks;static void check(bool v,const char*n){++checks;if(!v){fprintf(stderr,"PUMP_FAIL %s\n",n);exit(1);}}
static const char*status="{\"protocol\":3,\"session\":\"0123abcd\",\"ack\":0,\"busy\":false,\"ready\":true,\"heartbeat\":1,\"stage\":\"idle\"}";
static void ready(PixelController&c){c.set_exposure_mode(0);c.set_image_format(0);for(int t=0;t<2;t++){c.tick(t*250);for(unsigned n=0;n<8;n++){PixelRequest r;bool any=false;while(c.take_request(r)){any=true;const char*b="";if(r.endpoint==PixelEndpoint::Status)b=status;else if(r.endpoint==PixelEndpoint::Shutter)b="{\"protocol\":1,\"ready\":true,\"arm\":\"\",\"pending\":false,\"wait_press\":0}";else if(r.endpoint==PixelEndpoint::QuarterStatus)b="QUARTER_NATIVE_READY\n";else if(r.endpoint==PixelEndpoint::Quarter||r.endpoint==PixelEndpoint::Arm)b="DISARMED\n";c.response(r.id,0,b);}if(!any)break;}}check(c.state().can_start()&&c.state().nativeReady,"ready");}
struct Fake {bool blocked=false;std::vector<X2dPixelIoRequest> requests;std::vector<X2dPixelIoReply> replies;std::vector<uint64_t> cancellations;
 PixelIoApi api(){return {this,[](void*p,const X2dPixelIoRequest*r){auto&f=*static_cast<Fake*>(p);if(f.blocked)return 0;f.requests.push_back(*r);return 1;},[](void*p,X2dPixelIoReply*r){auto&f=*static_cast<Fake*>(p);if(f.replies.empty())return 0;*r=f.replies.front();f.replies.erase(f.replies.begin());return 1;},[](void*p,uint64_t id){static_cast<Fake*>(p)->cancellations.push_back(id);return 1;}};}
 void reply(uint64_t id,int result=0,const char*body=""){X2dPixelIoReply r{};r.id=id;r.status=result;r.length=std::strlen(body);std::memcpy(r.body,body,r.length);replies.push_back(r);}
};
static const X2dPixelIoRequest&arm(Fake&io){for(auto&r:io.requests)if(r.write&&r.endpoint==X2D_PIXEL_ARM)return r;std::abort();}
int main(){
 {PixelController c;ready(c);Fake io;io.blocked=true;PixelIoPump p(c,io.api());check(c.apply_pixel(),"apply");p.tick(250);check(p.pending_request()&&io.requests.empty(),"queue-full-preserves-pending");p.tick(1000);check(p.pending_request()&&io.requests.empty(),"held-across-ticks");io.blocked=false;p.tick(1100);check(!io.requests.empty()&&arm(io).write,"exact-arm-submitted-once");check(arm(io).deadline_ms==5250,"held-does-not-extend-original-deadline");check(std::string(arm(io).body,arm(io).length)=="PO3 0123abcd 1 6 2 0 START END\n","body-preserved");unsigned writes=0;for(auto&r:io.requests)writes+=r.write;check(writes==1,"no-write-retry");io.reply(arm(io).id);p.tick(1200);check(!c.state().sending,"reply-correlated");}
 {PixelController c;ready(c);Fake io;io.blocked=true;PixelIoPump p(c,io.api());check(c.apply_pixel(),"held-timeout-apply");p.tick(250);p.tick(5250);io.blocked=false;p.tick(5300);bool wrote=false;for(auto&r:io.requests)wrote|=r.write;check(!wrote&&!c.state().sending&&!c.state().applied,"expired-unsent-write-not-dispatched");}
 {PixelController c;ready(c);Fake io;PixelIoPump p(c,io.api());check(c.apply_pixel(),"inflight-apply");p.tick(250);auto id=arm(io).id;p.tick(5250);bool found=false;for(auto n:io.cancellations)found|=n==id;check(found&&c.state().sending,"inflight-cancel-does-not-fake-rollback");io.reply(id,409);p.tick(5300);check(!c.state().sending,"cancel-reply-clears-uncertain-write");}
 {PixelController c;ready(c);Fake io;PixelIoPump p(c,io.api());check(c.apply_pixel(),"session-change-apply");p.tick(250);auto id=arm(io).id;ProtocolObject o;check(parse_protocol("{\"protocol\":3,\"session\":\"89abcdef\",\"ack\":0,\"busy\":false,\"ready\":true,\"heartbeat\":2,\"stage\":\"idle\"}",o)&&c.accept_status(o),"session-changed");p.tick(300);bool found=false;for(auto n:io.cancellations)found|=n==id;check(found,"old-session-submission-cancelled");}
 {PixelController c;ready(c);Fake io;io.blocked=true;PixelIoPump p(c,io.api());check(c.apply_pixel(),"suspend-held-apply");p.tick(250);p.suspend();check(!p.pending_request()&&!c.state().sending,"suspend-proven-unsent");io.blocked=false;p.tick(300);bool wrote=false;for(auto&r:io.requests)wrote|=r.write;check(!wrote,"resume-no-old-held-write");}
 {PixelController c;ready(c);Fake io;PixelIoPump p(c,io.api());check(c.apply_pixel(),"suspend-before-first-tick");p.suspend();p.tick(300);bool wrote=false;for(auto&r:io.requests)wrote|=r.write;check(!wrote,"untaken-write-discarded");}
 {PixelController c;ready(c);Fake io;PixelIoPump p(c,io.api());check(c.apply_pixel(),"suspend-inflight-apply");p.tick(250);auto id=arm(io).id;p.suspend();auto count=io.cancellations.size();p.suspend();check(count>0&&io.cancellations.size()==count&&c.state().sending,"suspend-once-unknown-not-success");io.reply(id,409);p.tick(300);check(!c.state().sending,"suspend-receipt-drained");}
 printf("NATIVE_PIXEL_IO_PUMP_PASS assertions=%d filesystem=fixture camera_requests=0\n",checks);
}
