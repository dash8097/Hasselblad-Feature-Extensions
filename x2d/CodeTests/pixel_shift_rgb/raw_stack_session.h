#ifndef RAW_STACK_SESSION_H
#define RAW_STACK_SESSION_H
#include <stdint.h>
typedef enum {RS_IDLE,RS_PREPARED,RS_CAPTURING,RS_FINISHING,RS_OUTPUT,RS_COMPLETE,RS_FAILED} RsPhase;
typedef struct {
 RsPhase phase;
 uint64_t duration_ms,started_ms,deadline_ms,frames,last_frame_ms,gap_ms;
 uint64_t request,acknowledged;
 uint32_t exposure_id;
 unsigned in_flight,claimed,finish_requested,metadata_ready;
} RsSession;
int rs_session_prepare(RsSession *,uint32_t total_seconds);
/* 手动启动是唯一允许建立截止时刻的入口；prepare 不拍摄。 */
int rs_session_start(RsSession *,uint64_t monotonic_ms);
int rs_session_next(RsSession *,uint64_t monotonic_ms,uint64_t *request);
int rs_session_claim(RsSession *,uint64_t request,uint32_t exposure_id);
int rs_session_frame_done(RsSession *,uint64_t request,uint32_t exposure_id,
 uint64_t monotonic_ms,int metadata_ready);
void rs_session_finish(RsSession *);
int rs_session_output_ready(RsSession *,uint64_t monotonic_ms);
int rs_session_complete(RsSession *,int raw_saved,int compressed_saved,int restored);
void rs_session_fail(RsSession *);
#endif
