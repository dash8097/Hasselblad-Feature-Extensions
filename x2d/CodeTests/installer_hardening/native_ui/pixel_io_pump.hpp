#pragma once
#include "pixel_controller.hpp"
#include "pixel_file_io.h"
namespace x2dui {
struct PixelIoApi {void*context;int(*submit)(void*,const X2dPixelIoRequest*);int(*poll)(void*,X2dPixelIoReply*);int(*cancel)(void*,uint64_t);};
// Main-thread adapter only; the supplied implementation executes all filesystem
// work on its owned worker. Queue-full keeps the exact request/id for next tick.
class PixelIoPump {
 PixelController&controller;PixelIoApi io;PixelRequest pending;bool held=false;
 struct Flight {uint64_t id=0;bool cancelSent=false;};Flight flight[8]{};
 uint64_t lastTick=0;bool ticked=false;
public:
 PixelIoPump(PixelController&c,PixelIoApi p):controller(c),io(p){}
 void tick(uint64_t monotonicMs);
 void suspend();
 bool pending_request()const{return held;}
};
PixelIoApi pixel_io_api(X2dPixelIo*);
}
