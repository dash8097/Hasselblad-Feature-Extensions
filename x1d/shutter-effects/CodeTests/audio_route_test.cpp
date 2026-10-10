#include "../audio_route.h"
#include <cassert>
struct Fake {
    int value=1,level=0,writes=0,levelWrites=0;
    bool readable=true,writable=true,levelWritable=true;
    bool read(int &out){out=value;return readable;}
    bool write(int next){if(!writable)return false;value=next;++writes;return true;}
    bool readLevel(int &out){out=level;return readable;}
    bool writeLevel(int next){if(!levelWritable)return false;level=next;++levelWrites;return true;}
};
int main(){
    {Fake io;X1dAudioRoute<Fake> route(io);assert(route.acquire(3) && io.value==0 && io.level==3);assert(!route.acquire(3));assert(route.release() && io.value==1 && io.level==0);assert(route.release() && io.writes==2);}
    // 冷启动时路由与音量均为零，不等于被外部占用。
    {Fake io;io.value=0;X1dAudioRoute<Fake> route(io);assert(route.acquire(3) && io.level==3);assert(route.release() && io.value==0 && io.level==0);}
    {Fake io;io.readable=false;X1dAudioRoute<Fake> route(io);assert(!route.acquire(3) && io.writes==0 && io.levelWrites==0);}
    {Fake io;io.writable=false;X1dAudioRoute<Fake> route(io);assert(!route.acquire(3));io.writable=true;assert(route.release() && io.level==0);}
    {Fake io;X1dAudioRoute<Fake> route(io);assert(route.acquire(3));route.yieldToFactory();assert(route.release() && io.writes==1 && io.value==0 && io.level==3);}
    {Fake io;X1dAudioRoute<Fake> route(io);assert(route.acquire(3));io.writable=false;assert(!route.release());io.writable=true;assert(route.release() && io.value==1 && io.level==0);}
    {Fake io;X1dAudioRoute<Fake> route(io);assert(!route.acquire(0) && !route.acquire(-1) && !route.acquire(4));assert(io.writes==0 && io.levelWrites==0);}
    {Fake io;io.level=2;X1dAudioRoute<Fake> route(io);assert(route.acquire(1));io.levelWritable=false;assert(!route.release());io.levelWritable=true;assert(route.release() && io.level==2);}
    {Fake io;io.levelWritable=false;X1dAudioRoute<Fake> route(io);assert(!route.acquire(3) && io.writes==0);}
}
