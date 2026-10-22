/* Linux 4.9 ARM64 perf ABI 的有界解析器；不包含任何设备控制。 */
#ifndef X2D_SENSOR_PERF_CORE_H
#define X2D_SENSOR_PERF_CORE_H
#include <stdint.h>
#include <stddef.h>

#define SENSOR_SAMPLE_TYPE ((1ull<<0)|(1ull<<1)|(1ull<<2)|(1ull<<7)|(1ull<<12))
#define SENSOR_REG_MASK ((1ull<<0)|(1ull<<1)|(1ull<<8)|(1ull<<19)|(1ull<<20)|(1ull<<30))
#define SENSOR_SAMPLE_BYTES 96u
typedef struct {
    uint32_t type,size, pid,tid,cpu,ppid,ptid;
    uint64_t ip,time_ns,regs[6],lost;
} SensorRecord;

/* 内核 little-endian 数据独立按字节读取，避免环绕和未对齐指针。 */
static uint64_t sensor_le(const unsigned char *p,unsigned n) {
    uint64_t v=0;for(unsigned i=0;i<n;i++)v|=(uint64_t)p[i]<<(i*8);return v;
}
/* 1=样本，2=丢样，3=任务创建/退出，0=缺完整记录，负数=不兼容/损坏。 */
static int sensor_record(const unsigned char *p,size_t n,SensorRecord *r) {
    if(n<8)return 0;
    uint32_t type=(uint32_t)sensor_le(p,4),size=(uint32_t)sensor_le(p+6,2);
    if(size<8 || size>4096 || (size&7))return -1;
    if(n<size)return 0;
    *r=(SensorRecord){0};r->type=type;r->size=size;
    if(type==2){
        if(size!=24)return -2;r->lost=sensor_le(p+16,8);return 2;
    }
    if(type==4||type==7){
        if(size!=32)return -2;
        r->pid=(uint32_t)sensor_le(p+8,4);r->tid=(uint32_t)sensor_le(p+16,4);
        r->ppid=(uint32_t)sensor_le(p+12,4);r->ptid=(uint32_t)sensor_le(p+20,4);
        r->time_ns=sensor_le(p+24,8);return 3;
    }
    if(type!=9)return -3; /* throttle、未知记录均停止，不能忽略覆盖缺口。 */
    if(size!=SENSOR_SAMPLE_BYTES || sensor_le(p+40,8)!=2)return -4;
    if((sensor_le(p+4,2)&7)!=2)return -6; /* PERF_RECORD_MISC_USER */
    r->ip=sensor_le(p+8,8);r->pid=(uint32_t)sensor_le(p+16,4);
    r->tid=(uint32_t)sensor_le(p+20,4);r->time_ns=sensor_le(p+24,8);
    r->cpu=(uint32_t)sensor_le(p+32,4);
    if(!r->pid || !r->tid || !r->time_ns)return -5;
    for(unsigned i=0;i<6;i++)r->regs[i]=sensor_le(p+48+i*8,8);
    return 1;
}
/* 数据头使用 acquire；数据尾只有在消费成功后才由调用者 release 发布。 */
static int sensor_ring_copy(const unsigned char *ring,size_t size,
                            uint64_t head,uint64_t tail,unsigned char out[4096],size_t *n) {
    if(size<128 || (size&(size-1)) || head<tail || head-tail>size)return -1;
    uint64_t available=head-tail;if(available<8){*n=0;return 0;}
    unsigned char header[8];
    for(unsigned i=0;i<8;i++)header[i]=ring[(tail+i)&(size-1)];
    size_t count=(size_t)sensor_le(header+6,2);
    if(count<8 || count>4096 || (count&7) || count>size)return -2;
    if(count>available){*n=0;return 0;}
    for(size_t i=0;i<count;i++)out[i]=ring[(tail+i)&(size-1)];
    *n=count;return 1;
}

/* 不用 C bitfield，显式绑定 v4.9 的偏移；size=96 包含 clockid。 */
typedef struct {
    uint32_t type,size;
    uint64_t config,period,sample_type,read_format,flags;
    uint32_t wakeup_events,bp_type;
    uint64_t bp_addr,bp_len,branch_sample_type,sample_regs_user;
    uint32_t sample_stack_user;
    int32_t clockid;
} SensorPerfAttr;
_Static_assert(sizeof(SensorPerfAttr)==96,"perf attr v3");
_Static_assert(offsetof(SensorPerfAttr,flags)==40,"perf flags");
_Static_assert(offsetof(SensorPerfAttr,bp_addr)==56,"perf bp_addr");
_Static_assert(offsetof(SensorPerfAttr,sample_regs_user)==80,"perf regs");
_Static_assert(offsetof(SensorPerfAttr,clockid)==92,"perf clock");
static SensorPerfAttr sensor_attr(uint64_t address) {
    SensorPerfAttr a={0};a.type=5;a.size=96;a.period=1;
    a.sample_type=SENSOR_SAMPLE_TYPE;
    a.flags=(1ull<<0)|(1ull<<2)|(1ull<<5)|(1ull<<6)|(1ull<<13)|(1ull<<25);
    a.wakeup_events=1;a.bp_type=4;a.bp_addr=address;a.bp_len=4;
    a.sample_regs_user=SENSOR_REG_MASK;a.clockid=1;return a;
}
/* 这里传入 ELF VA；映射起点不一定等于 load bias。 */
static int sensor_load_bias(uint64_t start,uint64_t end,uint64_t mapping_offset,
                            uint64_t segment_va,uint64_t segment_offset,uint64_t site_va,
                            uint64_t *bias,uint64_t *address) {
    if(mapping_offset!=segment_offset||start<segment_va||start>=end||site_va<segment_va||
       start>=(1ull<<48)||end>=(1ull<<48))return 0;
    uint64_t b=start-segment_va;
    if(site_va>UINT64_MAX-b||b+site_va>=end)return 0;
    *bias=b;*address=b+site_va;return 1;
}
#endif
