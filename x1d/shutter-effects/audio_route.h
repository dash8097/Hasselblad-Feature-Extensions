#ifndef HBL_X1D_AUDIO_ROUTE_H
#define HBL_X1D_AUDIO_ROUTE_H

// 抽象替身约定：先设置音量，再切换逻辑路由；路由值不是占用锁。
// 播放仲裁由调用方负责；结束恢复现场，外部抢占时交还恢复责任。
template<class Io> class X1dAudioRoute {
    Io &io;
    bool owned=false;
    int previousRoute=0,previousLevel=0;
public:
    explicit X1dAudioRoute(Io &backend):io(backend) {}
    bool acquire(int level) {
        if(owned || level<1 || level>3)return false;
        if(!io.read(previousRoute) || !io.readLevel(previousLevel))return false;
        if(!io.writeLevel(level))return false;
        owned=true;
        if(!io.write(0)){release();return false;}
        return true;
    }
    bool release() {
        if(!owned)return true;
        if(!io.write(previousRoute))return false;
        if(!io.writeLevel(previousLevel))return false;
        owned=false;return true;
    }
    void yieldToFactory(){owned=false;}
};
#endif
