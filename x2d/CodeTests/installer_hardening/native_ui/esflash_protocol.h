#ifndef X2D_ESFLASH_PROTOCOL_H
#define X2D_ESFLASH_PROTOCOL_H
#include <stdint.h>
#define X2D_ESFLASH_MAGIC 0x58454632u
#define X2D_ESFLASH_SOCKET "x2d-esflash-v2"
typedef struct {uint32_t magic,operation,sequence,profile,delay_us;} X2dFlashRequest;
typedef struct {uint32_t magic,sequence,profile,delay_us,requested,sampling,fault,attempts,skipped,error_code,error_no;} X2dFlashReply;
/* Bounded diagnostic identifiers only: no paths, memory or user data. Keep
 * order stable; unknown failures remain faults and use code 255. */
static inline unsigned x2d_flash_error_code(const char*reason){
 static const char*const names[]={"helper_hash","helper_load","helper_init","service_not_unique","sampling_target_identity","site_count","site_index","source_open","source_size","source_map","source_hash","maps_allocate","maps_read","mapping_bounds","mapping_ambiguous","instruction_read_open","live_instruction_mismatch","capture_mode_read_open","page_size","sampling_thread_directory","thread_change_limit","thread_site_coverage","event_limit","perf_event_open","perf_ring_map","perf_ring_layout","enable_failed","clock_start","target_identity_changed","disable_failed","unmap_failed","close_failed","capture_mode_read_close","pulse_close_unconfirmed","pulse_failed_no_retry","ring_overrun_or_partial","lost_samples","task_changed","sample_abi","sample_identity_or_order","batch_limit","sample_clock","ambiguous_event_order_or_pair","event_unavailable","poll_failed"};
 if(!reason)return 0;
 for(unsigned n=0;n<sizeof(names)/sizeof(*names);++n){const char*a=reason,*b=names[n];while(*a&&*a==*b){++a;++b;}if(!*a&&!*b)return n+1;}
 return 255;
}
static inline int x2d_flash_reply_valid(const X2dFlashReply*r){
 return r&&r->magic==X2D_ESFLASH_MAGIC&&r->profile<=4&&r->delay_us<=5000000&&
 r->requested<=1&&r->sampling<=1&&r->fault<=1&&
 (r->error_code<=45||r->error_code==255)&&r->error_no<=4095&&
 (!!r->fault==!!r->error_code)&&(r->fault||!r->error_no);
}
/* profile 是按原厂当前位深／快门选择的内部时序，不代表已发送灯端 HSS 模式指令。 */
static inline unsigned x2d_flash_default_delay(unsigned profile){return profile==2?160000u:profile==3||profile==4?370000u:168000u;}
static inline int x2d_flash_request_valid(const X2dFlashRequest*r,unsigned sequence){
 return r&&r->magic==X2D_ESFLASH_MAGIC&&r->operation<=1&&
   (!r->operation||(sequence!=UINT32_MAX&&r->sequence==sequence+1&&r->profile<=4&&r->delay_us<=5000000));
}
#endif
