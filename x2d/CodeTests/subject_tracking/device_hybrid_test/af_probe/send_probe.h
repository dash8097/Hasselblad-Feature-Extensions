#include "command_origin.h"
#include "send_hooks.h"
extern U pthread_self(void);
extern void*dlsym(void*,const char*);
static U old_memcpy,old_bridge;
static atomic_uint request_serial,command_serial,send_serial;
typedef struct {U producer;unsigned value,type_plus1,request;} SpeedChoice;
typedef struct {atomic_uintptr_t owner;CommandOrigin scope,selected;unsigned char selected_cmd[16];SpeedChoice choice;} TraceThread;
static TraceThread trace_threads[32];
static TraceThread*trace_thread(void){
 U id=pthread_self();if(!id)return 0;
 for(unsigned i=0;i<32;i++){
  U owner=atomic_load_explicit(&trace_threads[i].owner,memory_order_relaxed);
  if(owner==id)return &trace_threads[i];
  if(!owner&&atomic_compare_exchange_strong(&trace_threads[i].owner,&owner,id))return &trace_threads[i];
 }
 return 0;
}
static unsigned read32(const unsigned char*p){return p[0]|p[1]<<8|p[2]<<16|(unsigned)p[3]<<24;}
/* 固定 4.2.0 指令数据流：选速返回值直接进入以下 scan_produce 调用。
 * 只有调用点、请求和传入速度全部匹配才携带档位；不是找最近选速记录。 */
static void choice_record(U producer,unsigned type,unsigned value){
 TraceThread*t=trace_thread();if(t)t->choice=(SpeedChoice){producer,value,(type&65535)+1,atomic_load(&request_serial)};
}
static void speed_type_record(U caller,unsigned type,unsigned value,int result){
 U producer=caller==0x9067c?0x906a4:caller==0x90844?0x9086c:0;
 if(producer)choice_record(result==0?producer:0,type,value);
}
static unsigned cdaf_speed(unsigned c,unsigned type){
 U caller=relative((U)__builtin_return_address(0));
 unsigned value=((unsigned(*)(unsigned,unsigned))originals[19])(c,type);
 U producer=caller==0x62f88?0x62f98:caller==0x644a4?0x644d0:caller==0x64988?0x64998:0;
 choice_record(producer,type,value);return value;
}
static CommandOrigin source(U caller){
 CommandOrigin s={.caller=relative(caller),.serial=atomic_fetch_add(&command_serial,1)+1,.request=atomic_load(&request_serial),.phase=-1};
 U p=*(U*)(base+0xa2b0f0);if(p)s.phase=*(int*)(p+0x20);return s;
}
static int produce(unsigned index,U caller,U container,unsigned a,unsigned b){
 TraceThread*t=trace_thread();CommandOrigin saved={0};if(t){saved=t->scope;t->scope=source(caller);
  if(index==9&&t->choice.producer==relative(caller)&&t->choice.producer&&
     t->choice.request==t->scope.request&&t->choice.value==(b&65535))t->scope.speed_type_plus1=t->choice.type_plus1;
  t->choice=(SpeedChoice){0};}
 int result=index==11?((int(*)(U))originals[index])(container):((int(*)(U,unsigned,unsigned))originals[index])(container,a,b);
 if(t)t->scope=saved;return result;
}
static int scan_produce(U c,unsigned a,unsigned b){return produce(9,(U)__builtin_return_address(0),c,a,b);}
static int abs_produce(U c,unsigned a,unsigned b){return produce(10,(U)__builtin_return_address(0),c,a,b);}
static int stop_produce(U c){return produce(11,(U)__builtin_return_address(0),c,0,0);}
static int container_push(unsigned char*c,const unsigned char*cmd){
 TraceThread*t=trace_thread();
 if(c[0]<10&&c[2]<10){CommandOrigin s=t?t->scope:(CommandOrigin){0};origin_put((U)(c+4+16*c[2]),cmd,16,s);}
 return ((int(*)(void*,const void*))originals[14])(c,cmd);
}
static int container_pop(unsigned char*c,unsigned char*cmd){
 TraceThread*t=trace_thread();CommandOrigin s={0};unsigned char snapshot[16]={0};
 if(c[1]==0&&t)t->selected=(CommandOrigin){0};
 if(c[0]&&c[1]<10){for(unsigned i=0;i<16;i++)snapshot[i]=c[4+16*c[1]+i];s=origin_take((U)(c+4+16*c[1]),snapshot,16);}
 int result=((int(*)(void*,void*))originals[15])(c,cmd);
 // _get_focus_ctrl_cmd 只保留最后一个类型 1..7 的 focus 命令。
 if(result==0&&cmd[0]==1&&read32(cmd+4)>=1&&read32(cmd+4)<=7&&t){
  t->selected=s;
  if(!same_bytes(snapshot,cmd,16))t->selected=(CommandOrigin){0};
  for(unsigned i=0;i<16;i++)t->selected_cmd[i]=cmd[i];
 }
 return result;
}
static int set_scan(unsigned c,unsigned direction,unsigned speed){
 TraceThread*t=trace_thread();CommandOrigin saved={0};
 if(t){saved=t->scope;t->scope=(CommandOrigin){0};
  if(read32(t->selected_cmd+4)==3&&read32(t->selected_cmd+8)==direction&&read32(t->selected_cmd+12)==speed)t->scope=t->selected;
  t->selected=(CommandOrigin){0};}
 int result=((int(*)(unsigned,unsigned,unsigned))originals[16])(c,direction,speed);
 if(t)t->scope=saved;return result;
}
static int set_abs(unsigned c,unsigned position,unsigned setting){
 TraceThread*t=trace_thread();CommandOrigin saved={0};
 if(t){saved=t->scope;t->scope=(CommandOrigin){0};
  if(read32(t->selected_cmd+4)==1&&read32(t->selected_cmd+8)==position)t->scope=t->selected;
  t->selected=(CommandOrigin){0};}
 int result=((int(*)(unsigned,unsigned,unsigned))originals[17])(c,position,setting);
 if(t)t->scope=saved;return result;
}
/* 原厂通用命令入口在扫描分支内联了 setter，不能仅拦独立 setter。
 * _get_focus_ctrl_cmd 复制的是原容器命令偏移 4 的完整 12 字节。 */
static int set_command(unsigned c,const unsigned char*cmd){
 TraceThread*t=trace_thread();CommandOrigin saved={0};
 if(t){saved=t->scope;t->scope=(CommandOrigin){0};
  if(cmd&&same_bytes(t->selected_cmd+4,cmd,12))t->scope=t->selected;}
 int result=((int(*)(unsigned,const void*))originals[18])(c,cmd);
 if(t){t->scope=saved;t->selected=(CommandOrigin){0};}
 return result;
}
static void record_queue_copy(U caller,void*dest,U n){
 if(n==40&&(caller==rcam_base+0x10d678||caller==rcam_base+0x10d6bc)){
  TraceThread*t=trace_thread();origin_put((U)dest,dest,40,t?t->scope:(CommandOrigin){0});
 }
}
static void*copy_to_queue(void*dest,const void*src,U n){
 U caller=(U)__builtin_return_address(0);
 void*result=((void*(*)(void*,const void*,U))old_memcpy)(dest,src,n);
 record_queue_copy(caller,dest,n);
 return result;
}
static int bridge_send(U handle,const unsigned char*data,unsigned n){
 CommandOrigin s={0};int selected=n==40&&data&&data[0]==10&&data[1]==248&&(data[4]==0x1b||data[4]==0x1d);
 unsigned mode=0;int value=0;
 if(selected){s=origin_take((U)data,data,40);mode=data[4]==0x1b?(data[8]&15):16+data[8];value=(int16_t)((unsigned)data[9]*256+data[10]);}
 /* 编号在调用前取得：多线程返回先后不能被冒充为提交先后。 */
 unsigned order=selected?atomic_fetch_add(&send_serial,1)+1:0;uint64_t submitted=selected?now():0;
 int result=((int(*)(U,const void*,unsigned))old_bridge)(handle,data,n);
 if(selected)af_record((AfEvent){.us=submitted,.caller=s.caller,.kind=AF_LENS_SEND,.a=(int)mode,.b=value,
   .result=result,.flags=(int)s.serial,.phase=s.phase,.request=s.request,.send_order=order,.speed_type_plus1=s.speed_type_plus1});
 return result;
}
static int attach_sender(void){
 if(!same_bytes((void*)(rcam_base+0x10d664),queue_copy_bytes,sizeof(queue_copy_bytes))||
    !same_bytes((void*)(rcam_base+0xe28ec),queue_send_bytes,sizeof(queue_send_bytes)))return 8;
 old_memcpy=*(U*)(rcam_base+RCAM_MEMCPY_SLOT);old_bridge=*(U*)(rcam_base+RCAM_WRITE_SLOT);
 if(old_memcpy!=(U)dlsym(0,"memcpy")||old_bridge!=(U)dlsym(0,"duss_hal_x2bridge_write"))return 9;
 U page=(rcam_base+RCAM_MEMCPY_SLOT)&~(U)4095,last=(rcam_base+RCAM_WRITE_SLOT)&~(U)4095;
 if(page!=last||mprotect((void*)page,4096,3))return 10;
 __atomic_store_n((U*)(rcam_base+RCAM_MEMCPY_SLOT),(U)copy_to_queue,__ATOMIC_RELEASE);
 __atomic_store_n((U*)(rcam_base+RCAM_WRITE_SLOT),(U)bridge_send,__ATOMIC_RELEASE);
 if(mprotect((void*)page,4096,1)){
  __atomic_store_n((U*)(rcam_base+RCAM_MEMCPY_SLOT),old_memcpy,__ATOMIC_RELEASE);
  __atomic_store_n((U*)(rcam_base+RCAM_WRITE_SLOT),old_bridge,__ATOMIC_RELEASE);
  mprotect((void*)page,4096,1);return 11;
 }
 return 0;
}
