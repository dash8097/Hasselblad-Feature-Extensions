#include "precapture_frame420.h"
#include <string.h>
static uint32_t le32(const unsigned char *p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static int overlap(const void *a,size_t n,const void *b,size_t m) {
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
    return n>UINTPTR_MAX-x || m>UINTPTR_MAX-y || (x<y+m && y<x+n);
}
int pc_frame420_nv12(const void *desc,size_t desc_bytes,const void *mapped,size_t mapped_bytes,
    uint64_t timestamp,uint64_t sequence,uint64_t epoch,struct pc_frame *out) {
    if(!desc || desc_bytes<536 || !mapped || !mapped_bytes || !out ||
        overlap(out,sizeof(*out),desc,desc_bytes) || overlap(out,sizeof(*out),mapped,mapped_bytes))return PC_INVALID;
    const unsigned char *d=desc;
    if(le32(d+0x28)!=2 || le32(d+0x38)!=PC_WIDTH || le32(d+0x3c)!=PC_HEIGHT || le32(d+0x80)!=2)return PC_INVALID;
    size_t stride[2],offset[2],size[2];
    for(unsigned i=0;i<2;++i) {
        const unsigned char *p=d+0x40+i*16;
        stride[i]=le32(p);offset[i]=le32(p+4);size_t rows=le32(p+8);
        /* 原厂这些字段按有符号32位使用；拒绝负值、短平面及乘法溢出。 */
        if(stride[i]<PC_WIDTH || stride[i]>INT32_MAX || rows<PC_HEIGHT/(i+1) || rows>INT32_MAX ||
            offset[i]>INT32_MAX || stride[i]>SIZE_MAX/rows)return PC_INVALID;
        size[i]=stride[i]*rows;
        if(offset[i]>mapped_bytes || size[i]>mapped_bytes-offset[i])return PC_INVALID;
    }
    if(offset[0]<offset[1]+size[1] && offset[1]<offset[0]+size[0])return PC_INVALID;
    struct pc_frame frame={(const unsigned char *)mapped+offset[0],(const unsigned char *)mapped+offset[1],
        size[0],size[1],stride[0],stride[1],PC_WIDTH,PC_HEIGHT,timestamp,sequence,epoch};
    memcpy(out,&frame,sizeof(frame));return PC_OK;
}
