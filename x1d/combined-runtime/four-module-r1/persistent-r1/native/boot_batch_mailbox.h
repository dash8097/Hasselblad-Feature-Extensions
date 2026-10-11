#pragma once
#include "boot_batch_envelope.h"
/* 两任务间单槽候选：解码任务只校验/复制，诊断任务执行。
 * 所有存储必须位于本次已验证归属的堆块；还未绑定真实函数入口。
 */
struct HblBatchMailbox {
    uint32_t slot; /* 0空，1复制中，2就绪，3执行中 */
    uint32_t token;
    size_t length;
    struct HblBatchState state;
    unsigned char payload[268];
};
static void hbl_batch_put32(volatile unsigned char *p,uint32_t v) {
    for(unsigned i=0;i<4;++i)p[i]=(unsigned char)(v>>(8*i));
}
/* 返回0保留原帧，8表示替换为普通F4通知，-1表示拒绝。
 * n必须是原厂帧解码器真实返回值，不能用队列容量代替。
 */
static int hbl_batch_stage(struct HblBatchMailbox *box,unsigned char *frame,size_t n) {
    const unsigned char *body=0;size_t length=0;
    int type=hbl_batch_envelope(frame,n,&body,&length);
    if(type<=0)return type;
    if(!box || !box->token)return -1;
    uint32_t empty=0;
    if(!__atomic_compare_exchange_n(&box->slot,&empty,1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return -1;
    if(!box->state.session || box->state.failed) {__atomic_store_n(&box->slot,0,__ATOMIC_RELEASE);return -1;}
    for(size_t i=0;i<length;++i)box->payload[i]=body[i];
    box->length=length;
    // 短通知仅在批数据完全发布后返回给原解码流程。
    hbl_batch_put32(frame+4,box->token);
    __atomic_store_n(&box->slot,2,__ATOMIC_RELEASE);
    return 8;
}
/* 返回0时调用原读处理器。返回1时发送标准9字节F5回应。
 * 队列中的旧通知不能重复执行；消费权用CAS只取得一次。
 */
static int hbl_batch_consume(struct HblBatchMailbox *box,const struct HblBatchMemory *memory,
                            const unsigned char request[8],unsigned char reply[9]) {
    if(!box || !request || !reply || request[0]!=0xf4 || request[1] || request[2]!=5 || request[3]!=1 ||
       hbl_batch_u32(request+4)!=box->token)return 0;
    uint32_t ready=2;
    if(!__atomic_compare_exchange_n(&box->slot,&ready,3,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return 0;
    uint32_t sequence=box->state.sequence;
    int result=hbl_batch_execute(&box->state,memory,box->payload,box->length);
    reply[0]=0xf5;reply[1]=0;reply[2]=1;reply[3]=5;
    hbl_batch_put32(reply+4,sequence);reply[8]=(unsigned char)result;
    __atomic_store_n(&box->slot,0,__ATOMIC_RELEASE);
    return 1;
}
