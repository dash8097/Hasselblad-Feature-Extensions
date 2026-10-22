#pragma once
#include "protocol_text.hpp"
namespace x2dui {
struct ViewportTile {unsigned x=0,y=0,width=0,height=0;};
// Original QQuickImage status values. Readiness also requires the original LUT.
enum class ViewportStatus:unsigned {Null=0,Ready=1,Loading=2,Error=3};
class PixelViewportCore {
 ProtocolText source_,decode_;unsigned width_=0,height_=0,texture_=0,count_=0;
 uint64_t due_=0,requested_=0,generation_=0;bool armed_=false;
 unsigned char tileState_[256]{};
public:
 unsigned readyTiles=0,failedTiles=0,regionRequests=0;uint64_t regionReadyMs=0;
 static bool valid_source(ProtocolBytes,unsigned width,unsigned height)noexcept;
 bool set(ProtocolBytes,unsigned width,unsigned height,unsigned texture,uint64_t now);
 // Observe original tiles without submitting a decode.
 bool adopt(ProtocolBytes,unsigned width,unsigned height,unsigned texture,uint64_t now);
 bool tick(uint64_t now); // true only when this generation needs tile creation
 bool tile(unsigned,ViewportTile&)const noexcept;
 ProtocolText tile_source(unsigned)const;
 void observe(uint64_t generation,unsigned index,unsigned imageStatus,bool lutUsed,unsigned lutStatus,uint64_t now);
 ViewportStatus status()const noexcept;
 unsigned count()const{return count_;}unsigned texture_size()const{return texture_;}
 uint64_t generation()const{return generation_;}const ProtocolText&decode_source()const{return decode_;}
};
}
