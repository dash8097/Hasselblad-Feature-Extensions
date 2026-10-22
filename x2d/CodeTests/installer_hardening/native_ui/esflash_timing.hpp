#pragma once
#include "esflash_client.h"
#include "factory_proxy.hpp"
namespace x2dui {
// X2D 4.2.0: property encodings verified against the original UI.
// These are user-measured sync boundaries, not a lamp-mode command.
struct FlashExposure {
 bool valid=false, electronic=false, idle=false;
 int64_t depth=-1,tv=0;
 unsigned profile()const {
  if(!valid||!electronic)return 0;
  return depth==0?(tv>=32?2U:1U):(tv>=20?4U:3U);
 }
};
inline FlashExposure flash_exposure(const FactoryProxyProfile&p){
 FlashExposure e;int64_t before=-1,after=-1,shutter=-1,low=0,high=-1;
 e.valid=factory_read(p,FactoryField::ExposureStatus,before)&&
  factory_read(p,FactoryField::ImageBitDepth,e.depth)&&
  factory_read(p,FactoryField::ShutterCurrent,e.tv)&&
  factory_read(p,FactoryField::ShutterMin,low)&&factory_read(p,FactoryField::ShutterMax,high)&&
  factory_read(p,FactoryField::ElectronicShutter,shutter)&&
  factory_read(p,FactoryField::ExposureStatus,after)&&before==after&&
  (e.depth==0||e.depth==1)&&(shutter==0||shutter==1)&&low<=high&&e.tv>=low&&e.tv<=high;
 e.electronic=shutter==1;e.idle=e.valid&&after==0;return e;
}
// Lives with the GUI session, not the settings page. No tree traversal,
// property writes, synchronous IPC, or repeated SET when settings are unchanged.
class FlashTiming {
public:
 void tick(X2dFlashClient&c,const FlashExposure&e,uint64_t now){
  // Electronic shutter is an always-on feature. Reconcile a fresh service
  // reply before sending; unchanged settings never restart the sampler.
  if(!c.ready||c.pending||c.queued)return;
  if(e.valid&&!e.idle)return; // retain the running exposure's timing
  unsigned profile=e.profile(),delay=x2d_flash_default_delay(profile);
  if(c.state.profile==profile&&c.state.requested==(profile!=0)&&(!profile||c.state.delay_us==delay))return;
  // A sampler fault must still allow the normal mechanical-shutter OFF intent.
  // A later OFF->ON transition is a fresh setting; the service retains its
  // cleanup/fault gate. Never repeatedly SET an already requested faulty output.
  if(c.state.fault&&profile&&c.state.requested)return;
  x2d_flash_client_set(&c,profile,delay,now);
 }
};
}
