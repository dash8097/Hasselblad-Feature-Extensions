#include "liveview_preferences.hpp"
#include <deque>
#include <cstdlib>
using namespace x2dui;
static unsigned checks;
#define C(v) do{++checks;if(!(v)){std::fprintf(stderr,"LIVEVIEW_PREFERENCES_FAIL line=%d\n",__LINE__);std::exit(1);}}while(0)
struct X2dPixelIo{std::deque<X2dPixelIoRequest>requests;std::deque<X2dPixelIoReply>replies;bool stopped=false;unsigned canceled=0;} io;
extern "C" X2dPixelIo*x2d_pixel_io_create(){io={};return &io;}
extern "C" int x2d_pixel_io_submit(X2dPixelIo*,const X2dPixelIoRequest*q){C(q->endpoint==X2D_LIVEVIEW_PREFERENCES);io.requests.push_back(*q);return 1;}
extern "C" int x2d_pixel_io_poll(X2dPixelIo*,X2dPixelIoReply*r){if(io.replies.empty())return 0;*r=io.replies.front();io.replies.pop_front();return 1;}
extern "C" int x2d_pixel_io_cancel(X2dPixelIo*,uint64_t){io.canceled++;return 1;}
extern "C" void x2d_pixel_io_stop(X2dPixelIo*){io.stopped=true;}
extern "C" int x2d_pixel_io_release(X2dPixelIo*){return io.stopped;}
static void reply(int status,const char*text){C(!io.requests.empty());X2dPixelIoReply r{};r.id=io.requests.front().id;r.status=status;if(text){r.length=std::strlen(text);std::memcpy(r.body,text,r.length);}io.requests.pop_front();io.replies.push_back(r);}
int main(){
 {LiveviewPreferences p;p.tick(0);C(!p.ready());reply(404,nullptr);p.tick(1);C(p.ready()&&p.seconds()==0);C(!p.save(1,2));C(p.save(30,2));C(!p.ready()&&p.saving());C(!p.save(60,3));C(io.requests.front().write==1);reply(200,nullptr);p.tick(4);C(io.requests.front().write==0&&!p.ready());reply(200,"X2DLV1 30\n");p.tick(5);C(p.ready()&&p.seconds()==30&&!p.saving());p.stop();C(p.drain());}
 {LiveviewPreferences p;p.tick(0);reply(200,"X2DLV1 030\n");p.tick(1);C(p.error()&&!p.ready()&&p.seconds()==0);C(p.save(10,2));reply(200,nullptr);p.tick(3);reply(200,"X2DLV1 20\n");p.tick(4);C(p.error()&&!p.ready()&&p.seconds()==0);p.stop();C(p.drain());}
 {LiveviewPreferences p;p.tick(0);reply(200,"X2DLV1 60\n");p.tick(3000);C(p.error()&&!p.ready());C(p.save(0,3001));reply(200,nullptr);p.tick(3002);C(!p.ready());reply(200,"X2DLV1 0\n");p.tick(3003);C(p.ready()&&p.seconds()==0);p.stop();C(p.drain());}
 {LiveviewPreferences p;p.tick(0);p.tick(3000);C(p.error()&&io.canceled==1);reply(200,"X2DLV1 60\n");p.tick(3001);C(!p.ready());p.stop();C(p.drain());}
 for(const char*bad:{"X2DLV1 -1\n","X2DLV1 10","X2DLV1 60\nextra","X2DLV1 600\n"}){int n=123;C(!LiveviewPreferences::parse(bad,std::strlen(bad),n)&&n==123);}
 std::printf("LIVEVIEW_PREFERENCES_PASS assertions=%u camera_operations=0\n",checks);
}
