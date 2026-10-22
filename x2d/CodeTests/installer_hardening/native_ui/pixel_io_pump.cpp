#include "pixel_io_pump.hpp"
#include <cstring>
namespace x2dui {
static_assert(static_cast<int>(PixelEndpoint::Status)==X2D_PIXEL_STATUS&&static_cast<int>(PixelEndpoint::Shutter)==X2D_PIXEL_SHUTTER&&static_cast<int>(PixelEndpoint::QuarterStatus)==X2D_PIXEL_QUARTER_STATUS&&static_cast<int>(PixelEndpoint::Arm)==X2D_PIXEL_ARM&&static_cast<int>(PixelEndpoint::Quarter)==X2D_PIXEL_QUARTER&&static_cast<int>(PixelEndpoint::Finish)==X2D_PIXEL_FINISH,"endpoint contract");
PixelIoApi pixel_io_api(X2dPixelIo*p){return {p,[](void*c,const X2dPixelIoRequest*r){return x2d_pixel_io_submit(static_cast<X2dPixelIo*>(c),r);},[](void*c,X2dPixelIoReply*r){return x2d_pixel_io_poll(static_cast<X2dPixelIo*>(c),r);},[](void*c,uint64_t id){return x2d_pixel_io_cancel(static_cast<X2dPixelIo*>(c),id);}};}
void PixelIoPump::suspend(){if(held){controller.discard_unsent(pending.id);held=false;}PixelRequest unsent;for(unsigned n=0;n<8&&controller.take_request(unsent);n++)controller.discard_unsent(unsent.id);for(auto&f:flight)if(f.id&&!f.cancelSent){io.cancel(io.context,f.id);f.cancelSent=true;}}
void PixelIoPump::tick(uint64_t now){
 if(!ticked||now<lastTick||now-lastTick>=250){controller.tick(now);lastTick=now;ticked=true;}
 for(auto&f:flight)if(f.id&&!f.cancelSent&&(!controller.request_dispatchable(f.id)||now>=controller.request_deadline_ms(f.id))){io.cancel(io.context,f.id);f.cancelSent=true;}
 X2dPixelIoReply reply{};for(unsigned n=0;n<8;n++){int rc=io.poll(io.context,&reply);if(rc<=0)break;bool owned=false;for(auto&f:flight)if(f.id==reply.id){f={};owned=true;break;}if(!owned)continue;if(reply.length>sizeof(reply.body)){controller.response(reply.id,500,{});continue;}controller.response(reply.id,reply.status,{reply.body,reply.length});}
 for(unsigned n=0;n<8;n++){
  if(!held){if(!controller.take_request(pending))break;held=true;}
  if(!controller.request_dispatchable(pending.id)||now>=controller.request_deadline_ms(pending.id)){controller.discard_unsent(pending.id);held=false;continue;}
  if(!pending.body.valid()||pending.body.size()>512){controller.response(pending.id,500,{});held=false;continue;}
  Flight*slot=nullptr;for(auto&f:flight)if(!f.id){slot=&f;break;}if(!slot)break;
  X2dPixelIoRequest request{};request.id=pending.id;request.deadline_ms=controller.request_deadline_ms(pending.id);if(!request.deadline_ms){controller.discard_unsent(pending.id);held=false;continue;}request.endpoint=static_cast<int>(pending.endpoint);request.write=pending.write;request.length=pending.body.size();if(request.length)std::memcpy(request.body,pending.body.data(),request.length);
  int rc=io.submit(io.context,&request);if(rc==0)break;
  if(rc<0)controller.response(pending.id,500,{});else *slot={request.id,false};held=false;
 }
}
}
