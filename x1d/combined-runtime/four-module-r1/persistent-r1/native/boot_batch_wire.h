#pragma once
#include <stdint.h>
#include <stddef.h>
/* 候选消息体，外层原厂4字节头另计，保守总长不超过272（诊断队列单项287）。
 * 尚未分配真实操作码、安装接收入口或接入UART。
 * session必须由外部已验证的启动会话创建，不是认证凭据。
 * 接收端白名单必须绑定固件、阶段、堆归属及精确值；不能直接允许任意地址。
 */
struct HblBatchState { uint32_t session,sequence; int failed; };
struct HblBatchMemory {
    void *context;
    int (*allow)(void *,uint32_t opcode,uint32_t address,uint32_t value,uint32_t mask);
    int (*read)(void *,uint32_t,uint32_t *);
    int (*write)(void *,uint32_t,uint32_t);
};
/* 返回0成功，其余值均终止此会话，不自动重试。消息缓冲区必须在调用期间保持不可变。 */
static uint32_t hbl_batch_u32(const unsigned char *p) {
    return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
static int hbl_batch_execute(struct HblBatchState *s,const struct HblBatchMemory *m,const unsigned char *p,size_t bytes) {
    if(!s || s->failed)return 1;
    s->failed=1;
    if(!m || !m->allow || !m->read || !m->write || !p || bytes<28 || bytes>268)return 2;
    uint32_t session=hbl_batch_u32(p+4),seq=hbl_batch_u32(p+8),op=hbl_batch_u32(p+12);
    uint32_t count=hbl_batch_u32(p+16),base=hbl_batch_u32(p+20);
    if(hbl_batch_u32(p)!=0x31424248 || !s->session || session!=s->session || seq!=s->sequence ||
       seq==0xffffffffu || hbl_batch_u32(p+24) || !count)return 3;
    /* 早期接收器只编入比较路径，减少每次开机引导器的上传体积。 */
#ifdef HBL_BATCH_COMPARE_ONLY
    if(op!=1)return 5;
#endif
    if(op==1) {if(count>20 || base || bytes!=28+12*count)return 4;}
    else if(op==2) {if(count>60 || (base&3) || (uint64_t)base+4*count>0x100000000ull || bytes!=28+4*count)return 4;}
    else return 4;
    /* 整批先授权，后访问；末项非法时不能留下前几项写入。 */
    for(uint32_t i=0;i<count;++i) {
        const unsigned char *q=p+28+(op==1?12:4)*i;
        uint32_t a=op==1?hbl_batch_u32(q):base+4*i;
        uint32_t v=hbl_batch_u32(q+(op==1?4:0)),mask=op==1?hbl_batch_u32(q+8):0xffffffffu;
        if((a&3) || !mask || (v&~mask) || !m->allow(m->context,op,a,v,mask))return 5;
    }
    for(uint32_t i=0;i<count;++i) {
        const unsigned char *q=p+28+(op==1?12:4)*i;
        uint32_t a=op==1?hbl_batch_u32(q):base+4*i;
        uint32_t v=hbl_batch_u32(q+(op==1?4:0)),mask=op==1?hbl_batch_u32(q+8):0xffffffffu,actual=0;
        if(op==2 && !m->write(m->context,a,v))return 6;
        if(!m->read(m->context,a,&actual))return 7;
        if((actual&mask)!=v)return 8;
    }
    ++s->sequence;s->failed=0;return 0;
}
