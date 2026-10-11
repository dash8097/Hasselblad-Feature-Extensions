#ifndef NATIVE_CAPTURE_H
#define NATIVE_CAPTURE_H
#include "native_af.h"
enum { NC_RAW=1,NC_CV_FIFO,NC_POSITION_FIFO,NC_ACCEPTED,NC_CYCLE,NC_COMMAND };
/* tick 是本机观察时刻，尚不是曝光/镜头采样时刻。每个环仅由一个上下文写入。 */
struct NcRecord { u32 commit,kind,tick,generation,state,data[4]; };
struct NcCapture {
    u32 magic,abi,generation,raw_sequence,af_sequence,invalid,enabled,reserved;
    u32 control_sequence,reserved_header[3];
    struct NcRecord raw[64],af[128],control[32];
};
extern volatile struct NcCapture nc_capture;
void nc_raw(u32 a,u32 b,u32 mode);
void nc_cv_fifo(u32 value,u32 index);
void nc_position_fifo(s32 position,u32 index,u32 lens_sequence);
void nc_accepted(u32 count);
#endif
