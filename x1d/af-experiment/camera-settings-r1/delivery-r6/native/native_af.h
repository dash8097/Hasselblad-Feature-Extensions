/* 原厂 AF 局部辅助：输入必须来自同一帧的反差/位置/采样时刻。 */
#ifndef NATIVE_AF_H
#define NATIVE_AF_H
typedef unsigned int u32;
typedef signed int s32;
enum { NA_FULL_FRAME=1, NA_ALIGNED=2, NA_TIMESTAMPS=4, NA_REQUIRED=7, NA_SECONDARY=8 };
enum { NA_OK, NA_TOO_FEW, NA_METADATA, NA_TIME, NA_MOTION, NA_NOISE,
       NA_CURVATURE, NA_BEHIND, NA_UNCALIBRATED };
struct NaSample {
    s32 position;
    u32 cv, sample_tick, receive_tick, frame_id, generation, flags, native_count,secondary_cv;
};
struct NaTiming {
    /* processing_ticks 是 now 之后的剩余处理预算；过去的处理耗时已计入 age。 */
    u32 now, processing_ticks, command_ticks, calibrated;
    float braking_per_tick2;
    u32 command_in_flight;
    float commanded_velocity_bound;
    u32 command_sequence;
};
struct NaResult {
    u32 direction_valid, prediction_valid, reason, used,prediction_kind;
    float direction_metric, velocity_per_tick, frame_ticks, age_ticks;
    float residual, peak_position, peak_distance, lead_distance;
    float required_distance, safe_speed_ratio, position_step,prediction_velocity_per_tick;
};
void na_assess(const struct NaSample *samples,u32 count,const struct NaTiming *timing,
               struct NaResult *out);
#endif
