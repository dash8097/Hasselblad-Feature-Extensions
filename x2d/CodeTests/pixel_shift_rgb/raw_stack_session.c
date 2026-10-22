#include "raw_stack_session.h"
#include "raw_stack_core.h"
#include <limits.h>
int rs_session_prepare(RsSession *s,uint32_t seconds){
 if(!s||seconds<1||seconds>86399)return 0;
 *s=(RsSession){.phase=RS_PREPARED,.duration_ms=(uint64_t)seconds*1000};return 1;
}
int rs_session_start(RsSession *s,uint64_t now){
 if(!s||s->phase!=RS_PREPARED||!now||now>UINT64_MAX-s->duration_ms)return 0;
 s->started_ms=now;s->deadline_ms=now+s->duration_ms;s->phase=RS_CAPTURING;return 1;
}
void rs_session_finish(RsSession *s){
 if(s&&(s->phase==RS_CAPTURING||s->phase==RS_FINISHING)){
  s->finish_requested=1;s->phase=RS_FINISHING;
 }
}
int rs_session_next(RsSession *s,uint64_t now,uint64_t *request){
 if(!s||!request||now<s->started_ms||now<s->last_frame_ms||s->phase!=RS_CAPTURING||s->in_flight)return 0;
 if(now>=s->deadline_ms||s->frames>=RS_MAX_FRAMES||s->request>=RS_MAX_FRAMES){rs_session_finish(s);return 0;}
 if(s->request==UINT64_MAX){rs_session_fail(s);return 0;}
 if(s->frames)s->gap_ms=now-s->last_frame_ms;
 s->in_flight=1;s->claimed=0;s->request++;*request=s->request;return 1;
}
int rs_session_claim(RsSession *s,uint64_t request,uint32_t id){
 if(!s||!s->in_flight||s->claimed||request!=s->request||request<=s->acknowledged||
    (s->phase!=RS_CAPTURING&&s->phase!=RS_FINISHING))return 0;
 s->claimed=1;s->exposure_id=id;return 1;
}
int rs_session_frame_done(RsSession *s,uint64_t request,uint32_t id,uint64_t now,int metadata){
 if(!s||!s->in_flight||!s->claimed||s->frames>=RS_MAX_FRAMES||request!=s->request||id!=s->exposure_id||
    now<s->started_ms||now<s->last_frame_ms||(s->phase!=RS_CAPTURING&&s->phase!=RS_FINISHING))return 0;
 if(!s->frames&&!metadata){rs_session_fail(s);return 0;}
 s->last_frame_ms=now;s->metadata_ready|=metadata!=0;s->frames++;
 s->acknowledged=request;s->in_flight=s->claimed=0;
 if(now>=s->deadline_ms||s->frames>=RS_MAX_FRAMES)rs_session_finish(s);
 return 1;
}
int rs_session_output_ready(RsSession *s,uint64_t now){
 if(!s||now<s->started_ms)return 0;
 if(s->phase==RS_CAPTURING&&now>=s->deadline_ms)rs_session_finish(s);
 if(s->phase!=RS_FINISHING||s->in_flight)return 0;
 if(!s->frames||!s->metadata_ready){rs_session_fail(s);return 0;}
 s->phase=RS_OUTPUT;return 1;
}
int rs_session_complete(RsSession *s,int raw,int compressed,int restored){
 if(!s||s->phase!=RS_OUTPUT)return 0;
 if(!raw||!compressed||!restored){rs_session_fail(s);return 0;}
 s->phase=RS_COMPLETE;return 1;
}
void rs_session_fail(RsSession *s){if(s)s->phase=RS_FAILED;}
