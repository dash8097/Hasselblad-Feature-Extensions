#pragma once
#include <stdint.h>
#include <stddef.h>
#include "boot_batch_wire.h"
/* 离线协议候选。完整报文长度必须由接收层提供，不能从固定队列尾部猜测。
 * CRC仅检测传输损坏，不替代会话授权和逐字内存校验。
 * 普通F4请求8字节；扩展载荷使用HBB1消息体后附CRC32。
 */
static uint32_t hbl_batch_crc(const unsigned char *p,size_t n) {
    uint32_t c=~0u;
    for(size_t i=0;i<n;++i){c^=p[i];for(unsigned k=0;k<8;++k)c=(c>>1)^(0xedb88320u&(0u-(c&1)));}
    return ~c;
}
/* 0=普通消息，1=完整批量消息，-1=扩展损坏。不读取实际长度以外的数据。 */
static int hbl_batch_envelope(const unsigned char *p,size_t n,const unsigned char **body,size_t *length) {
    if(!p || !body || !length || n<4)return -1;
    *body=0;*length=0;
    if(p[0]!=0xf4 || p[1]!=0 || p[2]!=5 || p[3]!=1)return 0;
    if(n==8)return 0;
    if(n<36 || n>276)return -1;
    if(hbl_batch_u32(p+4)!=0x31424248)return -1;
    if(hbl_batch_crc(p,n-4)!=hbl_batch_u32(p+n-4))return -1;
    *body=p+4;*length=n-8;return 1;
}
