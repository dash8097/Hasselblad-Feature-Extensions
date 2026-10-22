#ifndef QUARTER_DECISION_WIRE_H
#define QUARTER_DECISION_WIRE_H
#include <stdint.h>
#define QD_DECISION_SOCKET "/tmp/x2d-quarter-decision.sock"
#define QD_DECISION_MAGIC 0x32445158u
/* 机内同端序的固定长度消息；不含照片元数据。不明曝光必须返回known=0。 */
typedef struct {uint32_t magic,exposure,known,quarter;} QdDecision;
static int qd_decision_valid(const QdDecision *r,uint32_t id){
 return r&&r->magic==QD_DECISION_MAGIC&&r->exposure==id&&r->known==1&&r->quarter==1;
}
#endif
