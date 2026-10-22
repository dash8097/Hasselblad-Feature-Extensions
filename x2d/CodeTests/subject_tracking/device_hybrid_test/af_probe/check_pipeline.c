#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "probe_core.h"
typedef uintptr_t U;
static unsigned char memory[0xa30000];
static U base,rcam_base,originals[20];
static uint64_t now(void){static unsigned n;return ++n;}
static U relative(U pc){return pc;}
static int mprotect(void*a,U n,int flags){(void)a;(void)n;(void)flags;return 0;}
U pthread_self(void){return 1;}
void*dlsym(void*a,const char*b){(void)a;(void)b;return 0;}
#include "send_probe.h"
static unsigned char packet[8][40];static int packet_index;
static void put32(unsigned char*p,unsigned n){for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(n>>(8*i));}
static int push(void*p,const void*cmd){unsigned char*c=p;if(c[0]>=10)return -1;memcpy(c+4+16*c[2],cmd,16);c[0]++;c[2]=(c[2]+1)%10;return 0;}
static int pop(void*p,void*cmd){unsigned char*c=p;if(!c[0])return -1;memcpy(cmd,c+4+16*c[1],16);c[1]++;c[0]--;return 0;}
static int scan_push(U p,unsigned d,unsigned v){unsigned char cmd[16]={1};put32(cmd+4,3);put32(cmd+8,d);put32(cmd+12,v);return container_push((void*)p,cmd);}
static int submit(unsigned c,unsigned direction,unsigned value){
 (void)c;(void)direction;
 unsigned char*p=packet[packet_index++];p[0]=10;p[1]=248;p[4]=0x1b;p[8]=5;
 /* 模拟后续适配改速，最终记录必须是 900，而非初始的 600。 */
 value+=300;p[9]=(unsigned char)(value>>8);p[10]=(unsigned char)value;
 record_queue_copy(rcam_base+0x10d678,p,40);return 0;
}
static int send_original(U h,const void*p,unsigned n){assert(h==123&&n==40);assert(((const unsigned char*)p)[4]==0x1b);return 0;}
static int inline_command(unsigned c,const void*p){const unsigned char*cmd=p;assert(read32(cmd)==3);return submit(c,read32(cmd+4),read32(cmd+8));}
int main(void){
 base=(U)memory;originals[9]=(U)scan_push;originals[14]=(U)push;originals[15]=(U)pop;originals[16]=(U)submit;old_bridge=(U)send_original;
 atomic_store(&request_serial,7);unsigned char c[164]={0},out[16];
 assert(produce(9,0xabc,(U)c,32767,600)==0);
 assert(container_pop(c,out)==0);assert(set_scan(0,32767,600)==0);
 memset(c,0,sizeof c);
 assert(produce(9,0xdef,(U)c,32767,600)==0);
 assert(container_pop(c,out)==0);assert(set_scan(0,32767,600)==0);
 /* 反序消费相同速度值：必须按命令地址对应，不准按最新阶段或速度匹配。 */
 assert(bridge_send(123,packet[1],40)==0);assert(bridge_send(123,packet[0],40)==0);
 AfSnapshot s;assert(af_snapshot(&s));assert(s.count==2);
 assert(s.events[0].caller==0xdef&&s.events[1].caller==0xabc);
 assert(s.events[0].b==900&&s.events[1].b==900&&s.events[0].request==7);
 /* 未匹配的 setter 参数不会继承旧阶段，实际下发仍会如实记录。 */
 assert(set_scan(0,32767,600)==0);assert(bridge_send(123,packet[2],40)==0);
 assert(af_snapshot(&s));assert(s.events[2].caller==0&&s.events[2].flags==0);
 /* 4.2.0 lens_ctrl_set_focus_cmd 内联扫描，完全不调用独立扫描 setter。 */
 originals[18]=(U)inline_command;memset(c,0,sizeof c);
 assert(produce(9,0x123,(U)c,32768,1200)==0);assert(container_pop(c,out)==0);
 assert(set_command(0,out+4)==0);assert(bridge_send(123,packet[3],40)==0);
 assert(af_snapshot(&s));assert(s.events[3].caller==0x123&&s.events[3].b==1500);
 /* 参数改变后拒绝错误绑定，也不能继承上一次来源。 */
 put32(out+12,1199);assert(set_command(0,out+4)==0);assert(bridge_send(123,packet[4],40)==0);
 assert(af_snapshot(&s));assert(s.events[4].caller==0);
 /* 档位随同一指令传到最终发送，即使适配器再次改速仍保留。 */
 memset(c,0,sizeof c);choice_record(0x64998,2,1200);
 assert(produce(9,0x64998,(U)c,32768,1200)==0);assert(container_pop(c,out)==0);
 assert(set_command(0,out+4)==0);assert(bridge_send(123,packet[5],40)==0);
 assert(af_snapshot(&s));assert(s.events[5].speed_type_plus1==3&&s.events[5].b==1500);
 /* 相同速度不同调用点不得借用档位。 */
 memset(c,0,sizeof c);choice_record(0x64998,1,1200);
 assert(produce(9,0x644d0,(U)c,32768,1200)==0);assert(container_pop(c,out)==0);
 assert(set_command(0,out+4)==0);assert(bridge_send(123,packet[6],40)==0);
 assert(af_snapshot(&s));assert(s.events[6].speed_type_plus1==0);
 /* 同一调用点但速度不符也保持未知。 */
 memset(c,0,sizeof c);choice_record(0x64998,0,1200);
 assert(produce(9,0x64998,(U)c,32768,1199)==0);assert(container_pop(c,out)==0);
 assert(set_command(0,out+4)==0);assert(bridge_send(123,packet[7],40)==0);
 assert(af_snapshot(&s));assert(s.events[7].speed_type_plus1==0);
 puts("PASS: producer -> container -> selected command -> downstream modified speed -> exact send buffer -> origin; mismatches remain unknown");
}
