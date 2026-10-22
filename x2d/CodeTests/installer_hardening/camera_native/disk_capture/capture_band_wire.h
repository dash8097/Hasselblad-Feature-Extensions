/* 固定本机 little-endian 通道；不传指针、原厂句柄或拍摄 metadata。
 * 每批确认代表显影器已停止使用该批数据；完整完成另需 FINISH 确认。 */
#ifndef CAPTURE_BAND_WIRE_H
#define CAPTURE_BAND_WIRE_H
#include <stdint.h>
#include <stddef.h>
#define CBW_MAGIC UINT64_C(0x3157444e41423258)
#define CBW_WIDTH 23326u
#define CBW_HEIGHT 17498u
#define CBW_FILE_ROWS 2048u
#define CBW_FILE_BATCHES ((CBW_HEIGHT+CBW_FILE_ROWS-1)/CBW_FILE_ROWS)
enum {CBW_HELLO=1,CBW_BAND=2,CBW_FINISH=3,CBW_ACK=4,CBW_NATIVE_HELLO=5,CBW_FILE_BAND=6,
 CBW_FULL_HELLO=7,CBW_FULL_READY=8};
#define CBW_FULL_BYTES ((uint64_t)CBW_WIDTH*CBW_HEIGHT*2)
typedef struct {uint64_t magic,task,handle,bytes;} CbwSharedResult;
_Static_assert(sizeof(CbwSharedResult)==32,"shared result layout differs");
#define CBW_METADATA_MAX (16u*1024u*1024u)
/* exFAT 不支持 socket inode；全局任务锁限制为一个活跃任务，bind 拒绝
 * 旧端点，双方另核对 SO_PEERCRED 与精确程序身份。 */
#define CBW_SOCKET_PATH "/tmp/x2d-pixel-overclock/memory.sock"
typedef struct {
 uint64_t magic,task,ticket,bytes;
 uint32_t operation,index,first_row,rows,buffer_first,buffer_rows,width,reserved;
} CbwPacket;
_Static_assert(sizeof(CbwPacket)==64,"band wire layout differs");
static inline int cbw_platform(void){const uint32_t v=1;return *(const uint8_t*)&v==1;}
static inline int cbw_hello(const CbwPacket *p){
 return p&&p->magic==CBW_MAGIC&&p->task&&!p->ticket&&!p->bytes&&
  p->operation==CBW_HELLO&&!p->index&&!p->first_row&&p->rows==CBW_HEIGHT&&
  !p->buffer_first&&!p->buffer_rows&&p->width==CBW_WIDTH&&!p->reserved;
}
/* 新内存入口必须携带真实 DCAM 容器；旧空 HELLO 不能启动新入口。 */
static inline int cbw_native_hello(const CbwPacket *p){
 if(!p||!p->bytes||p->bytes>CBW_METADATA_MAX||p->operation!=CBW_NATIVE_HELLO)return 0;
 CbwPacket plain=*p;plain.bytes=0;plain.operation=CBW_HELLO;return cbw_hello(&plain);
}
static inline int cbw_full_hello(const CbwPacket *p){
 if(!p||p->operation!=CBW_FULL_HELLO)return 0;
 CbwPacket native=*p;native.operation=CBW_NATIVE_HELLO;return cbw_native_hello(&native);
}
static inline int cbw_shared_result(const CbwSharedResult *p,uint64_t task){
 return p&&task&&p->magic==CBW_MAGIC&&p->task==task&&p->handle&&p->bytes==CBW_FULL_BYTES;
}
static inline int cbw_full_ready(const CbwPacket *p,uint64_t task){
 if(!p)return 0;
 CbwPacket expected={.magic=CBW_MAGIC,.task=task,.ticket=CBW_FILE_BATCHES,
  .operation=CBW_FULL_READY,.rows=CBW_HEIGHT,.buffer_rows=CBW_HEIGHT,.width=CBW_WIDTH};
 const uint8_t *a=(const uint8_t*)&expected,*b=(const uint8_t*)p;
 for(unsigned i=0;i<sizeof expected;i++)if(a[i]!=b[i])return 0;
 return task!=0;
}
static inline int cbw_band(const CbwPacket *p,uint64_t task,unsigned index,
 unsigned next,uint64_t last_ticket){
 if(!p||p->magic!=CBW_MAGIC||p->task!=task||p->operation!=CBW_BAND||
    p->index!=index||index>=9||p->first_row!=next||next>=CBW_HEIGHT||
    !p->ticket||p->ticket<=last_ticket||p->width!=CBW_WIDTH||p->reserved)return 0;
 unsigned rows=CBW_HEIGHT-next;if(rows>2048)rows=2048;
 unsigned first=next>96?next-96:0,end=next+rows+4;
 if(end>CBW_HEIGHT)end=CBW_HEIGHT;
 return p->rows==rows&&p->buffer_first==first&&p->buffer_rows==end-first&&
  p->bytes==(uint64_t)(end-first)*CBW_WIDTH*2;
}
static inline int cbw_ack(const CbwPacket *request,const CbwPacket *ack){
 if(!request||!ack)return 0;
 CbwPacket expected=*request;expected.operation=CBW_ACK;
 const uint8_t *a=(const uint8_t*)&expected,*b=(const uint8_t*)ack;
 for(unsigned i=0;i<sizeof expected;i++)if(a[i]!=b[i])return 0;
 return 1;
}
/* 串行显影读取最终 RAW：只交接范围，不再复制整批像素到 socket。 */
static inline int cbw_file_band(const CbwPacket *p,uint64_t task,unsigned index,
 unsigned next,uint64_t last_ticket){
 if(!p||p->operation!=CBW_FILE_BAND||p->bytes||p->magic!=CBW_MAGIC||p->task!=task||
    p->index!=index||index>=CBW_FILE_BATCHES||next>=CBW_HEIGHT||p->first_row!=next||
    !p->ticket||p->ticket<=last_ticket||p->width!=CBW_WIDTH||p->reserved)return 0;
 unsigned rows=CBW_HEIGHT-next;if(rows>CBW_FILE_ROWS)rows=CBW_FILE_ROWS;
 return p->rows==rows&&p->buffer_first==next&&p->buffer_rows==rows;
}
#endif
