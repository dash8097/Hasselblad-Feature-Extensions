#pragma once
#include "pixel_file_io.h"
#include "screen_policy.hpp"
#include <cstring>
#include <cstdio>
namespace x2dui {
// GUI侧只提交固定文件请求；保存后再次读回，成功前不启用新期限。
class LiveviewPreferences {
 X2dPixelIo* io_=nullptr;uint64_t sequence_=0,pending_=0,deadline_=0;
 int phase_=0,seconds_=0,wanted_=0;bool started_=false,ready_=false,error_=false,stopping_=false;
 bool submit(bool write,uint64_t now){
  if(!io_||now>UINT64_MAX-3000||sequence_==UINT64_MAX)return false;
  X2dPixelIoRequest q{};q.id=++sequence_;q.endpoint=X2D_LIVEVIEW_PREFERENCES;q.write=write;q.deadline_ms=now+3000;
  if(write){int n=std::snprintf(q.body,sizeof q.body,"X2DLV1 %d\n",wanted_);if(n<=0||size_t(n)>=sizeof q.body)return false;q.length=size_t(n);}
  if(!x2d_pixel_io_submit(io_,&q))return false;pending_=q.id;deadline_=q.deadline_ms;return true;
 }
 void fail(){if(io_&&pending_)x2d_pixel_io_cancel(io_,pending_);pending_=0;phase_=0;ready_=false;error_=true;seconds_=0;}
public:
 static bool parse(const char*data,size_t length,int&seconds){
  static const int values[]={0,10,20,30,60,120,300};
  for(int n:values){char text[32];int size=std::snprintf(text,sizeof text,"X2DLV1 %d\n",n);if(size>0&&length==size_t(size)&&!std::memcmp(data,text,length)){seconds=n;return true;}}
  return false;
 }
 void tick(uint64_t now){
  if(stopping_)return;
  if(!started_){started_=true;io_=x2d_pixel_io_create();phase_=1;if(!submit(false,now)){fail();return;}}
  X2dPixelIoReply r{};
  for(unsigned n=0;io_&&n<8&&x2d_pixel_io_poll(io_,&r);++n){
   if(!pending_||r.id!=pending_)continue;
   if(now>=deadline_){fail();continue;}
   pending_=0;
   if(phase_==2){if(r.status!=200){fail();continue;}phase_=3;if(!submit(false,now))fail();continue;}
   int value=0;bool valid=r.status==200&&parse(r.body,r.length,value);
   if(phase_==1&&r.status==404)valid=true;
   if(!valid||(phase_==3&&value!=wanted_)){fail();continue;}
   seconds_=value;ready_=true;error_=false;phase_=0;
  }
  if(pending_&&now>=deadline_)fail();
 }
 bool save(int seconds,uint64_t now){if(stopping_||phase_||!io_||!LiveviewTimeoutPolicy::valid_seconds(seconds))return false;wanted_=seconds;phase_=2;ready_=false;error_=false;if(!submit(true,now)){fail();return false;}return true;}
 bool ready()const{return ready_;}bool saving()const{return phase_==2||phase_==3;}bool error()const{return error_;}
 int seconds()const{return ready_?seconds_:0;}
 void stop(){stopping_=true;ready_=false;if(io_)x2d_pixel_io_stop(io_);}
 bool drain(){if(!stopping_)return false;if(io_&&!x2d_pixel_io_release(io_))return false;io_=nullptr;return true;}
};
}
