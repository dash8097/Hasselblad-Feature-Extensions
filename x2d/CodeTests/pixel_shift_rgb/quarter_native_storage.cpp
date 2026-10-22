/* 固定 X2D 100C 4.2.0 原厂单拍后台适配。只替换 FileIO 已入队输出，
 * 不发曝光、不接管快门、不创建拍摄会话、不暂停取景、不写原尺寸临时图。
 * Qt ABI/函数指纹由官方 ELF 自动生成；其他版本保持原厂并拒绝应用25MP。 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#ifndef QN_HOST_CHECK
#include <sys/mman.h>
#include <pthread.h>
#include <dlfcn.h>
#include <link.h>
#else
#include <windows.h>
typedef SRWLOCK pthread_mutex_t;
#define PTHREAD_MUTEX_INITIALIZER SRWLOCK_INIT
static void pthread_mutex_lock(pthread_mutex_t *p){AcquireSRWLockExclusive(p);}
static void pthread_mutex_unlock(pthread_mutex_t *p){ReleaseSRWLockExclusive(p);}
extern void *qn_mock_symbol(uintptr_t);
extern int qn_mock_selected;
#endif
extern "C" {
#include "quarter_native_output.h"
}
#include "quarter_native_jpeg.h"
#include "quarter_software_jpeg.h"
#include "quarter_decision_wire.h"
#include "capture_preview_stream.h"
#include "quarter_native_bindings.inc"
struct Shared {void *value,*control;};
struct Array {void *allocation,*data;intptr_t size;};
struct Piece {Array label;Shared memory;};
struct Call {uintptr_t function;void *result;uintptr_t args[8];};
extern "C" void x2d_qt_native_call(const Call *);
extern "C" void qn_prepare_entry(void);
extern "C" void qn_fullsize_entry(void);
extern "C" void qn_metadata_entry(void);
static uintptr_t base,prepare_original,metadata_original;
static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
struct Exposure {uint32_t id;unsigned valid,quarter;uint64_t age;uint32_t raw_bytes,jpeg_bytes;};
static Exposure exposures[4096];static uint64_t clock_id;
static unsigned raw_done,jpeg_done,failures;
static void metrics(unsigned type,int ok,int reason){
#ifndef QN_HOST_CHECK
 pthread_mutex_lock(&mutex);
 char line[160];int n=snprintf(line,sizeof line,"{\"raw\":%u,\"jpeg\":%u,\"failed\":%u,\"last_type\":%u,\"last_ok\":%s,\"reason\":%d}\n",raw_done,jpeg_done,failures,type,ok?"true":"false",reason);
 int f=open("/tmp/x2d-quarter-native.metrics",O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW,0600);
 if(f>=0){if(n>0&&n<(int)sizeof line)(void)write(f,line,n);close(f);}
 pthread_mutex_unlock(&mutex);
#else
 (void)type;(void)ok;(void)reason;
#endif
}
#ifndef QN_HOST_CHECK
#define FN(off,type) ((type)(base+(off)))
static void status(const char *value){
 int f=open("/tmp/x2d-quarter-native.status",O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW,0600);
 if(f>=0){(void)write(f,value,strlen(value));close(f);}
}
static int selected(){
 char v[32];int f=open("/tmp/x2d-pixel-overclock/quarter.txt",O_RDONLY|O_NOFOLLOW);if(f<0)return 0;
 ssize_t n=read(f,v,sizeof v);close(f);return n==11&&!memcmp(v,"QN1 25 END\n",11);
}
#else
#define FN(off,type) ((type)qn_mock_symbol(off))
static int selected(){return qn_mock_selected;}
#endif
static void snapshot(uint32_t id){
 pthread_mutex_lock(&mutex);
 unsigned slot=0;uint64_t oldest=UINT64_MAX;
 for(unsigned i=0;i<4096;i++){
  if(exposures[i].valid&&exposures[i].id==id){pthread_mutex_unlock(&mutex);return;}
  if(!exposures[i].valid){slot=i;oldest=0;}else if(oldest&&exposures[i].age<oldest){slot=i;oldest=exposures[i].age;}
 }
 exposures[slot]={id,1,(unsigned)selected(),++clock_id,0,0};pthread_mutex_unlock(&mutex);
}
static void commit_size(uint32_t id,unsigned type,uint32_t bytes){
 pthread_mutex_lock(&mutex);
 for(unsigned i=0;i<4096;i++)if(exposures[i].valid&&exposures[i].id==id){
  if(type==1)exposures[i].raw_bytes=bytes;else if(type==4)exposures[i].jpeg_bytes=bytes;break;
 }
 pthread_mutex_unlock(&mutex);
}
static uint32_t output_size(uint32_t id,unsigned type){
 uint32_t out=0;pthread_mutex_lock(&mutex);
 for(unsigned i=0;i<4096;i++)if(exposures[i].valid&&exposures[i].id==id){
  out=type==1?exposures[i].raw_bytes:type==4?exposures[i].jpeg_bytes:0;break;
 }
 pthread_mutex_unlock(&mutex);return out;
}
static int quarter(uint32_t id){
 int out=0;pthread_mutex_lock(&mutex);
 for(unsigned i=0;i<4096;i++)if(exposures[i].valid&&exposures[i].id==id){out=exposures[i].quarter;break;}
 pthread_mutex_unlock(&mutex);return out;
}
static int qn_decision_reply(QdDecision *q){
 if(!q||q->magic!=QD_DECISION_MAGIC)return 0;
 q->known=q->quarter=0;pthread_mutex_lock(&mutex);
 for(unsigned i=0;i<4096;i++)if(exposures[i].valid&&exposures[i].id==q->exposure){
  q->known=1;q->quarter=exposures[i].quarter;break;
 }
 pthread_mutex_unlock(&mutex);return 1;
}
static int receive(void *service,const void *info,const Shared *notification){
 if(info)snapshot(*(const uint32_t*)info);
 return FN(B_RECEIVE,int(*)(void*,const void*,const Shared*))(service,info,notification);
}
static void release(Shared *v){if(v->control)FN(B_SHARED_DESTROY,void(*)(Shared*))(v);*v={};}
static Shared copy(Shared p){if(p.control){__atomic_fetch_add((int*)p.control,1,__ATOMIC_RELAXED);__atomic_fetch_add((int*)p.control+1,1,__ATOMIC_RELAXED);}return p;}
static void string_release(Array *a){
 if(a->allocation&&__atomic_sub_fetch((int*)a->allocation,1,__ATOMIC_ACQ_REL)==0)
  FN(B_ARRAY_FREE,void(*)(void*,intptr_t,intptr_t))(a->allocation,2,8);
 *a={};
}
static void list_release(Array *a){
 if(a->allocation&&__atomic_sub_fetch((int*)a->allocation,1,__ATOMIC_ACQ_REL)==0){
  Piece *p=(Piece*)a->data;for(intptr_t i=0;i<a->size;i++){release(&p[i].memory);string_release(&p[i].label);}
  FN(B_ARRAY_FREE,void(*)(void*,intptr_t,intptr_t))(a->allocation,sizeof(Piece),8);
 }*a={};
}
struct Owner {Shared p{};~Owner(){release(&p);} };
struct List {Array a{};~List(){list_release(&a);} };
struct Config {
 Shared p{};
 Config(){
  p.value=FN(B_NEW,void*(*)(size_t))(16);memset(p.value,0,16);
  try{p.control=FN(B_NEW,void*(*)(size_t))(24);}
  catch(...){FN(B_DELETE,void(*)(void*))(p.value);p.value=nullptr;throw;}
  int *counts=(int*)p.control;counts[0]=counts[1]=1;
  ((uintptr_t*)p.control)[1]=base+B_CONFIG_DELETE;((void**)p.control)[2]=p.value;
  try{FN(B_DEST_FORMAT,void(*)(void*,unsigned))(p.value,8);}
  catch(...){release(&p);throw;}
 }
 ~Config(){release(&p);}
};
static uint8_t *data(Shared p){
 if(!p.value)return nullptr;
 FN(B_CPU_SYNC,void(*)(void*))(p.value);
 return ((uint8_t*(*)(void*))(*(uintptr_t*)(*(uintptr_t*)p.value+0x68)))(p.value);
}
static size_t capacity(Shared p){return p.value?((uint32_t(*)(void*))(*(uintptr_t*)(*(uintptr_t*)p.value+0x88)))(p.value):0;}
static int allocate(Owner &out,size_t bytes){
 if(!bytes||bytes>UINT32_MAX)return 0;
 uint32_t n=(uint32_t)bytes,heap=0,flags=0x3f;Array name{};
 Call c{base+B_DUSS_CREATE,&out.p,{(uintptr_t)&n,(uintptr_t)&heap,(uintptr_t)&flags,(uintptr_t)&name}};
 x2d_qt_native_call(&c);return out.p.value&&out.p.control&&capacity(out.p)>=bytes&&data(out.p);
}
static void append(List &out,Owner &owner){
 Piece piece{{},owner.p};owner.p={};
 try{FN(B_APPEND,void(*)(Array*,intptr_t,Piece*))(&out.a,out.a.size,&piece);}
 catch(...){release(&piece.memory);string_release(&piece.label);throw;}
 release(&piece.memory);string_release(&piece.label);
}
static int append_borrow(List &out,Shared p){Owner o;o.p=copy(p);append(out,o);return 1;}
static int raw_convert(const Array *source,List &out,int64_t &bytes){
 if(source->size<3||source->size>RS_MAX_SEGMENTS+2)return 0;
 Piece *p=(Piece*)source->data;
 if(capacity(p[0].memory)!=16384)return 0;
 uint8_t *header=data(p[0].memory);if(!header)return 0;
 RsPixelBlock blocks[RS_MAX_SEGMENTS];unsigned count=0;uint64_t total=0;
 unsigned preview_index=0;
 for(intptr_t i=1;i<source->size&&total<210510336;i++){
  size_t n=capacity(p[i].memory);uint8_t *input=data(p[i].memory);
  if(!input||!n||(n&1)||count==RS_MAX_SEGMENTS)return 0;
  if(n>210510336-total){if(n-(210510336-total)>4095)return 0;n=(size_t)(210510336-total);}
  blocks[count++]={(const uint16_t*)input,n};total+=n;preview_index=(unsigned)i+1;
 }
 if(total!=210510336||preview_index+1!=(unsigned)source->size)return 0;
 size_t preview_capacity=capacity(p[preview_index].memory);
 uint8_t *preview=data(p[preview_index].memory);
 size_t valid_preview=capture_preview_stream_size(preview,preview_capacity);
 if(!valid_preview)return 0;
 Owner raw,head,preview_tail;
 if(!allocate(raw,52627584))return 0;
 size_t raw_capacity=capacity(raw.p);QnRawPlan plan;
 if(!qn_raw_plan_storage(header,raw_capacity,valid_preview,preview_capacity,&plan))return 0;
 Shared final_preview=p[preview_index].memory;
 if(plan.padding){
  size_t padded=preview_capacity+plan.padding;
  if(!allocate(preview_tail,padded)||capacity(preview_tail.p)!=padded)return 0;
  uint8_t *target=data(preview_tail.p);memcpy(target,preview,preview_capacity);
  memset(target+preview_capacity,0,plan.padding);
  FN(B_DEVICE_SYNC,void(*)(void*))(preview_tail.p.value);
  if(!qn_raw_plan_storage(header,raw_capacity,valid_preview,padded,&plan))return 0;
  final_preview=preview_tail.p;
 }
 QnRawInput input{11904,8842,11904,count,blocks};
 QnRawOutput output{(uint16_t*)data(raw.p),raw_capacity,5952,4421,5952};
 if(!qn_raw_convert(&input,&output,4))return 0;
 memset((uint8_t*)output.pixels+plan.raw_bytes,0,raw_capacity-plan.raw_bytes);
 FN(B_DEVICE_SYNC,void(*)(void*))(raw.p.value);
 if(!allocate(head,16384)||capacity(head.p)!=16384)return 0;
 memcpy(data(head.p),plan.header,16384);FN(B_DEVICE_SYNC,void(*)(void*))(head.p.value);
 append(out,head);append(out,raw);append_borrow(out,final_preview);
 if(plan.padding)return 0;
 bytes=(int64_t)plan.total_bytes;return 1;
}
static int jpeg_convert(const Array *source,List &out,int64_t &bytes){
 if(source->size<2||source->size>32)return -1;
 Piece *p=(Piece*)source->data;
 size_t header_bytes=capacity(p[0].memory),total=0;
 if(header_bytes<2||header_bytes>1024*1024)return -2;
 for(intptr_t i=1;i<source->size;i++){size_t n=capacity(p[i].memory);if(!n||n>128u*1024*1024-total)return -3;total+=n;}
 /* 工人自己的解码输入，不能把原厂共享JPEG首两字节改回去。 */
 Owner incoming,encoded;Array name{},planes{};int dims[2]={11656,8742};
 Call alloc{base+B_IMAGE_ALLOCATE,&incoming.p,{8,(uintptr_t)dims,(uintptr_t)&name,0,total,0,0x37,(uintptr_t)&planes}};
 x2d_qt_native_call(&alloc);
 if(!incoming.p.value||capacity(incoming.p)<total)return -4;
 uint8_t *joined=data(incoming.p);if(!joined)return -5;size_t at=0;
 for(intptr_t i=1;i<source->size;i++){size_t n=capacity(p[i].memory);uint8_t *input=data(p[i].memory);if(!input)return -6;memcpy(joined+at,input,n);at+=n;}
 joined[0]=255;joined[1]=216;
 uint8_t *original_header=data(p[0].memory);if(!original_header)return -7;
 uint8_t *prefix=(uint8_t*)malloc(header_bytes);if(!prefix)return -8;
 struct FreePrefix {void *p;~FreePrefix(){free(p);}} own_prefix{prefix};
 size_t prefix_bytes=qj_copy_prefix(original_header,header_bytes,prefix,header_bytes,5828,4371);
 if(!prefix_bytes)return -9;
 unsigned input_w=0,input_h=0;
 if(!qj_stream(joined,total,&input_w,&input_h))return -10;
 uint8_t *jpeg=joined;size_t n=total;
 struct SoftOutput {uint8_t *p=nullptr;~SoftOutput(){free(p);}} soft;
 if(input_w==5828&&(input_h==4371||input_h==4372)){
  /* 上游已经直接编码25MP：只整理私有码流和Exif，绝不再进ImageTransform。 */
  if(!qj_quarter_visible(joined,total))return -11;
 }else{
  if(input_w!=11656||input_h!=8742)return -12;
  /* camera-storage的ImageTransform明确不支持JPEG解码；使用有界行流式IDCT。 */
  if(!qn_jpeg_half(joined,total,&soft.p,&n))return -18;
  jpeg=soft.p;
 }
 if(!qj_quarter_visible(jpeg,n))return -15;
 unsigned w=0,h=0;size_t valid=qj_stream(jpeg,n,&w,&h);
 if(!valid||w!=5828||h!=4371)return -15;
 size_t final_size=(prefix_bytes+valid-2+4095)&~(size_t)4095;
 Owner final;if(!allocate(final,final_size)||capacity(final.p)!=final_size)return -16;
 uint8_t *target=data(final.p);memset(target,0,final_size);
 memcpy(target,prefix,prefix_bytes);memcpy(target+prefix_bytes,jpeg+2,valid-2);
 FN(B_DEVICE_SYNC,void(*)(void*))(final.p.value);
 append(out,final);bytes=(int64_t)final_size;return 1;
}
extern "C" void qn_prepare(Array *result,const Shared *work,int64_t *bytes,uintptr_t caller){
 Call original{prepare_original,result,{(uintptr_t)work,(uintptr_t)bytes}};x2d_qt_native_call(&original);
 if(caller!=base+B_WRITE_CALLER||!work||!work->value||!bytes||!result->size)return;
 void *cache=*(void**)((uint8_t*)work->value+0x10);if(!cache)return;
 uint32_t id=FN(B_EXPOSURE_ID,uint32_t(*)(void*))(cache);
 if(!quarter(id))return;
 unsigned type=*((uint8_t*)work->value+0x38);
 if(type!=1&&type!=4&&type!=6)return;
 List replacement;int64_t length=0;int ok=0;
 try{ok=type==1?raw_convert(result,replacement,length):type==4?jpeg_convert(result,replacement,length):0;}
 catch(...){ok=0;}
 /* HEIF没有经验证的缩小编码绑定：拒绝写入伪25MP。UI限定RAW/JPEG。 */
 int reason=ok<0?-ok:0;ok=ok>0;
 list_release(result);
 if(ok){*result=replacement.a;replacement.a={};*bytes=length;commit_size(id,type,(uint32_t)length);
  __atomic_fetch_add(type==1?&raw_done:&jpeg_done,1,__ATOMIC_RELAXED);
 }else{*bytes=-1;__atomic_fetch_add(&failures,1,__ATOMIC_RELAXED);}
 metrics(type,ok,reason);
}
/* std::shared_ptr<ImageInfo>由原厂创建；只改它新返回的对象，不改控制块。 */
extern "C" void qn_fullsize(Shared *out,void *content,const Array *path,unsigned type){
 Call call{base+B_FULLSIZE,out,{(uintptr_t)content,(uintptr_t)path,type}};x2d_qt_native_call(&call);
 uint32_t bytes=output_size(*(uint32_t*)((uint8_t*)content+0x90),type);
 if(!out->value||!bytes)return;
 uint8_t *info=(uint8_t*)out->value;
 int *size=(int*)(info+16);size[0]=type==1?5952:5828;size[1]=type==1?4421:4371;
 if(type==1){*(uint32_t*)(info+24)=5952;*(uint32_t*)(info+28)=52627584;}
 else{*(uint32_t*)(info+8)=0;*(uint32_t*)(info+28)=bytes;}
}
/* 文件重新打开时原厂RAW分支有固定一亿尺寸；用该文件的stride和
 * StripByteCounts识别25MP，不靠当前模式或旧曝光编号。 */
extern "C" void qn_metadata(Shared *out,void *metadata,unsigned type){
 Call call{metadata_original,out,{(uintptr_t)metadata,type}};x2d_qt_native_call(&call);
 if(type!=1||!out->value)return;
 uint8_t *info=(uint8_t*)out->value;
 if(*(uint32_t*)(info+8)==16384&&*(uint32_t*)(info+24)==5952&&*(uint32_t*)(info+28)==52627584){
  ((int*)(info+16))[0]=5952;((int*)(info+16))[1]=4421;
 }
}
struct Rect {int left,top,right,bottom;};
static Rect crop(void *content,const Array *path,unsigned type){
 Rect r=FN(B_CROP,Rect(*)(void*,const Array*,unsigned))(content,path,type);
 if(output_size(*(uint32_t*)((uint8_t*)content+0x90),type)&&r.right>=r.left&&r.bottom>=r.top){
  if(type==1){r.left=62;r.top=46;r.right=62+5828-1;r.bottom=46+4371-1;}
  else{r.left=0;r.top=0;r.right=5828-1;r.bottom=4371-1;}
 }return r;
}
#ifndef QN_HOST_CHECK
#ifdef QN_DIRECT_JPEG_BRIDGE
#include "quarter_decision_server.inc"
#endif
static int image_base(struct dl_phdr_info *image,size_t,void *out){
 if(!image->dlpi_name[0]||!strcmp(image->dlpi_name,"/system/bin/camera-storage")){*(uintptr_t*)out=image->dlpi_addr;return 1;}return 0;
}
static int replace(uintptr_t address,const void *value,size_t n){
 int f=open("/proc/self/mem",O_RDWR);if(f<0)return 0;
 int ok=lseek(f,(off_t)address,SEEK_SET)==(off_t)address&&write(f,value,n)==(ssize_t)n;close(f);
 if(ok)__builtin___clear_cache((char*)address,(char*)address+n);
 return ok&&!memcmp((void*)address,value,n);
}
static int pointer(uintptr_t address,uintptr_t value){return replace(address,&value,8);}
__attribute__((constructor))static void init(){
 const char *enable=getenv("X2D_QUARTER_NATIVE");if(!enable||strcmp(enable,"1"))return;
 char path[128];ssize_t n=readlink("/proc/self/exe",path,sizeof path-1);
 if(n<=0||n>=(ssize_t)sizeof path-1)return;path[n]=0;
 if(strcmp(path,"/system/bin/camera-storage"))return;
 status("QUARTER_NATIVE_PROFILE_CHECK\n");dl_iterate_phdr(image_base,&base);
 const char *(*version)()=(const char*(*)())dlsym(nullptr,"qVersion");
 if(!base||!version||strcmp(version(),"6.4.1")){status("QUARTER_NATIVE_PROFILE_REJECTED\n");return;}
 for(unsigned i=0;i<sizeof(qn_bindings)/sizeof(*qn_bindings);i++)
  if(memcmp((void*)(base+qn_bindings[i].address),qn_bindings[i].bytes,16)){status("QUARTER_NATIVE_BINDING_REJECTED\n");return;}
 uintptr_t receive_slot=base+0x249090+16+0x178,full_slot=base+0x2489e0,crop_slot=base+0x2489d0;
 if(*(uintptr_t*)receive_slot!=base+B_RECEIVE||*(uintptr_t*)full_slot!=base+B_FULLSIZE||*(uintptr_t*)crop_slot!=base+B_CROP){status("QUARTER_NATIVE_VTABLE_REJECTED\n");return;}
 void *trampoline=mmap(nullptr,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
 if(trampoline==MAP_FAILED){status("QUARTER_NATIVE_TRAMPOLINE_FAILED\n");return;}
 memcpy(trampoline,(void*)(base+B_PREPARE),16);
 uint32_t jump[4]={0x58000051,0xd61f0220,0,0};uintptr_t resume=base+B_PREPARE+16;
 memcpy(jump+2,&resume,8);memcpy((uint8_t*)trampoline+16,jump,16);
 memcpy((uint8_t*)trampoline+64,(void*)(base+B_METADATA),16);
 resume=base+B_METADATA+16;memcpy(jump+2,&resume,8);memcpy((uint8_t*)trampoline+80,jump,16);
 __builtin___clear_cache((char*)trampoline,(char*)trampoline+96);
 if(mprotect(trampoline,4096,PROT_READ|PROT_EXEC)){munmap(trampoline,4096);status("QUARTER_NATIVE_TRAMPOLINE_FAILED\n");return;}
 prepare_original=(uintptr_t)trampoline;metadata_original=(uintptr_t)trampoline+64;
 uintptr_t entry=(uintptr_t)qn_prepare_entry;memcpy(jump+2,&entry,8);
 int received=0,full=0,cropped=0;
 if((received=pointer(receive_slot,(uintptr_t)receive))&&(full=pointer(full_slot,(uintptr_t)qn_fullsize_entry))&&
    (cropped=pointer(crop_slot,(uintptr_t)crop))&&replace(base+B_PREPARE,jump,16)){
  entry=(uintptr_t)qn_metadata_entry;memcpy(jump+2,&entry,8);
  if(replace(base+B_METADATA,jump,16)){
#ifdef QN_DIRECT_JPEG_BRIDGE
   if(!qn_start_decisions()){status("QUARTER_NATIVE_READY_DIRECT_BRIDGE_UNAVAILABLE\n");return;}
#endif
   status("QUARTER_NATIVE_READY\n");return;
  }
 }
 /* write失败也可能只改了一部分，所有尝试过的槽都恢复原字节。 */
 replace(base+B_PREPARE,qn_prepare_original_bytes,16);replace(base+B_METADATA,qn_metadata_original_bytes,16);
 pointer(crop_slot,base+B_CROP);pointer(full_slot,base+B_FULLSIZE);pointer(receive_slot,base+B_RECEIVE);
 status("QUARTER_NATIVE_HOOK_FAILED\n");
}
#endif
extern "C" __attribute__((visibility("default"))) int qn_storage_metrics(unsigned *raw,unsigned *jpeg,unsigned *failed){
 if(!raw||!jpeg||!failed)return 0;
 *raw=__atomic_load_n(&raw_done,__ATOMIC_RELAXED);*jpeg=__atomic_load_n(&jpeg_done,__ATOMIC_RELAXED);*failed=__atomic_load_n(&failures,__ATOMIC_RELAXED);return 1;
}
