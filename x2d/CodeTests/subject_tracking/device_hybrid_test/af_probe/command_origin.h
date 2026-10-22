/* 仅保存旁路元数据；不改原厂命令。冲突/覆盖/内容不符一律未知，不推测。 */
#ifndef COMMAND_ORIGIN_H
#define COMMAND_ORIGIN_H
#include <stdint.h>
#include <stdatomic.h>
typedef struct {uint64_t caller;unsigned serial,request;int phase;unsigned speed_type_plus1;} CommandOrigin;
typedef struct {uintptr_t key;unsigned epoch,n;unsigned char bytes[40];CommandOrigin origin;} OriginSlot;
typedef struct {atomic_flag lock;atomic_uint epoch,lost;OriginSlot slots[256];} OriginMap;
static OriginMap origins={.lock=ATOMIC_FLAG_INIT};
static int same_bytes(const void*a,const void*b,unsigned n){const unsigned char*x=a,*y=b;for(unsigned i=0;i<n;i++)if(x[i]!=y[i])return 0;return 1;}
static unsigned origin_index(uintptr_t p){return (unsigned)((p>>4)^(p>>12))&255;}
static int origin_lock(void){
 if(!atomic_flag_test_and_set_explicit(&origins.lock,memory_order_acquire))return 1;
 atomic_fetch_add(&origins.epoch,1);atomic_fetch_add(&origins.lost,1);return 0;
}
static void origin_put(uintptr_t key,const void*bytes,unsigned n,CommandOrigin origin){
 if(n>40||!origin_lock())return;
 OriginSlot*s=&origins.slots[origin_index(key)];
 if(s->key&&s->key!=key)atomic_fetch_add(&origins.lost,1);
 s->key=key;s->n=n;s->origin=origin;s->epoch=atomic_load(&origins.epoch);
 for(unsigned i=0;i<n;i++)s->bytes[i]=((const unsigned char*)bytes)[i];
 atomic_flag_clear_explicit(&origins.lock,memory_order_release);
}
static CommandOrigin origin_take(uintptr_t key,const void*bytes,unsigned n){
 CommandOrigin result={0};if(!origin_lock())return result;
 OriginSlot*s=&origins.slots[origin_index(key)];
 if(s->key==key&&s->n==n&&s->epoch==atomic_load(&origins.epoch)&&same_bytes(s->bytes,bytes,n))result=s->origin;
 if(s->key==key)s->key=0;
 atomic_flag_clear_explicit(&origins.lock,memory_order_release);return result;
}
#endif
