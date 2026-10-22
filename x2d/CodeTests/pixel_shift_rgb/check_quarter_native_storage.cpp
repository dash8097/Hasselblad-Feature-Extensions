/* 同一工人适配源在自造Qt/native替身下验证；无照片或设备操作。 */
#define QN_HOST_CHECK 1
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "quarter_native_storage.cpp"
#include <assert.h>
#undef assert
#define assert(v) do{if(!(v)){fprintf(stderr,"ASSERT %d: %s\n",__LINE__,#v);fflush(stderr);exit(2);}}while(0)
int qn_mock_selected;
static unsigned owners,original_calls,receives,encode_calls;
static int metadata_small;
static int fail_alloc,fail_encode,fail_sync,fail_append,append_after_encode;
static HANDLE encoding,continue_encoding;
struct MockMem {uint8_t object[0xb0];uint8_t *pixels;size_t bytes;};
struct MockControl {int weak,strong;uintptr_t deleter;void *object;};
static uintptr_t vtable[20];
static uint8_t *ptr(void *m){return ((MockMem*)m)->pixels;}
static uint32_t len(void *m){return (uint32_t)((MockMem*)m)->bytes;}
static Shared memory(size_t n){
 if(fail_alloc)throw 4;
 MockMem *m=(MockMem*)calloc(1,sizeof *m);assert(m);m->pixels=(uint8_t*)calloc(1,n);assert(m->pixels);m->bytes=n;
 *(uintptr_t*)m->object=(uintptr_t)vtable;
 MockControl *c=(MockControl*)calloc(1,sizeof *c);assert(c);c->weak=c->strong=1;c->object=m;owners++;
 return {m,c};
}
static void shared_destroy(Shared *p){
 if(!p->control)return;
 MockControl *c=(MockControl*)p->control;
 if(__atomic_sub_fetch(&c->strong,1,__ATOMIC_ACQ_REL)==0){
  if(c->deleter==B_CONFIG_DELETE)free(c->object);
  else{MockMem *m=(MockMem*)c->object;free(m->pixels);free(m);owners--;}
 }
 if(__atomic_sub_fetch(&c->weak,1,__ATOMIC_ACQ_REL)==0)free(c);
}
static void array_free(void *v,intptr_t unit,intptr_t){if(unit==40)free(*(void**)((uint8_t*)v+8));free(v);}
static void append_mock(Array *out,intptr_t at,Piece *p){
 assert(at==out->size);if(fail_append)throw 5;
 if(!out->allocation){out->allocation=calloc(1,16);*(int*)out->allocation=1;}
 out->data=realloc(out->data,(out->size+1)*sizeof(Piece));assert(out->data);
 ((Piece*)out->data)[out->size++]=*p;*p={};*(void**)((uint8_t*)out->allocation+8)=out->data;
}
static void sync_mock(void*){if(fail_sync)throw 6;}
static void setsize(void *m,uint32_t n){*(uint32_t*)((uint8_t*)m+0xa8)=n;}
static uint32_t exposure_id(void *c){return *(uint32_t*)c;}
static int receive_mock(void*,const void*,const Shared*){receives++;return 1;}
static void setdest(void*,unsigned v){assert(v==8);}
static Rect crop_mock(void*,const Array*,unsigned){return {124,92,11779,8833};}
void *qn_mock_symbol(uintptr_t a){
 switch(a){
  case B_NEW:return (void*)malloc;case B_DELETE:return (void*)free;
  case B_SHARED_DESTROY:return (void*)shared_destroy;case B_ARRAY_FREE:return (void*)array_free;
  case B_APPEND:return (void*)append_mock;case B_CPU_SYNC:case B_DEVICE_SYNC:return (void*)sync_mock;
  case B_DEST_FORMAT:return (void*)setdest;case B_IMAGE_SET_SIZE:return (void*)setsize;
  case B_EXPOSURE_ID:return (void*)exposure_id;case B_RECEIVE:return (void*)receive_mock;case B_CROP:return (void*)crop_mock;
 }assert(0);return nullptr;
}
static size_t jpeg(uint8_t *p,unsigned w,unsigned h){
 const uint8_t start[]={255,216,255,192,0,17,8,0,0,0,0,3,1,0x21,0,2,0x11,1,3,0x11,1,255,218,0,12,3,1,0,2,0x11,3,0x11,0,63,0,3,4,255,0,9,255,217};
 memcpy(p,start,sizeof start);qj_put(p+7,h,1,2);qj_put(p+9,w,1,2);return sizeof start;
}
extern "C" int qn_jpeg_half(const uint8_t *in,size_t bytes,uint8_t **out,size_t *size){
 encode_calls++;unsigned w=0,h=0;assert(qj_stream(in,bytes,&w,&h)&&w==11656&&h==8742);
 if(encoding){SetEvent(encoding);assert(WaitForSingleObject(continue_encoding,10000)==WAIT_OBJECT_0);}
 if(fail_encode)return 0;*out=(uint8_t*)calloc(1,4096);assert(*out);*size=jpeg(*out,5828,4371);
 if(append_after_encode)fail_append=1;return 1;
}
struct MockWork {uint8_t work[96];uint32_t id;List original;};
extern "C" void x2d_qt_native_call(const Call *c){
 if(c->function==prepare_original){
  original_calls++;MockWork *w=(MockWork*)*(void**)c->args[0];List out;
  Piece *p=(Piece*)w->original.a.data;for(intptr_t i=0;i<w->original.a.size;i++)append_borrow(out,p[i].memory);
  *(Array*)c->result=out.a;out.a={};*(int64_t*)c->args[1]=123;return;
 }
 if(c->function==B_DUSS_CREATE){*(Shared*)c->result=memory(*(uint32_t*)c->args[0]);return;}
 if(c->function==B_IMAGE_ALLOCATE){
  assert(c->args[0]==8&&c->args[3]==0&&c->args[5]==0&&c->args[6]==0x37);
  *(Shared*)c->result=memory(c->args[4]);return;
 }
 if(c->function==B_TRANSFORM){
  encode_calls++;Shared *in=(Shared*)c->args[0];int *size=(int*)c->args[1];assert(size[0]==5828&&size[1]==4372);
  unsigned w=0,h=0;assert(qj_stream(ptr(in->value),len(in->value),&w,&h)&&w==11656&&h==8742);
  if(encoding){SetEvent(encoding);assert(WaitForSingleObject(continue_encoding,10000)==WAIT_OBJECT_0);}
  if(fail_encode)throw 7;
  Shared m=memory(4096);size_t n=jpeg(ptr(m.value),5828,4372);setsize(m.value,(uint32_t)n);*((uint8_t*)m.value+0x7c)=8;
  *(Shared*)c->result=m;if(append_after_encode)fail_append=1;return;
 }
 if(c->function==B_FULLSIZE||c->function==metadata_original){
  Shared *v=(Shared*)c->result;v->value=calloc(1,32);v->control=nullptr;
  int *dims=(int*)((uint8_t*)v->value+16);dims[0]=11904;dims[1]=8842;*(uint32_t*)((uint8_t*)v->value+28)=metadata_small?52627584:210510336;
  *(uint32_t*)((uint8_t*)v->value+8)=16384;*(uint32_t*)((uint8_t*)v->value+24)=metadata_small?5952:11904;return;
 }
 assert(0);
}
static void add(MockWork &w,Shared m){Owner o;o.p=m;append(w.original,o);}
static void make_work(MockWork &w,unsigned type,uint32_t id){
 memset(w.work,0,sizeof w.work);w.work[0x38]=(uint8_t)type;w.id=id;*(void**)(w.work+0x10)=&w.id;
 qn_mock_selected=1;receive(nullptr,&w.id,nullptr);
}
static void header(uint8_t *h){
 memcpy(h,"II*\0",4);qj_put(h+4,8,0,4);
 const uint32_t main[][4]={{330,4,1,256},{273,4,1,210526720},{279,4,1,42}};
 const uint32_t raw[][4]={{256,4,1,11904},{257,4,1,8842},{258,3,1,16},{259,3,1,1},{273,4,1,16384},{278,4,1,8842},{279,4,1,210510336},{50719,4,2,600},{50720,4,2,608}};
 for(unsigned k=0;k<2;k++){
  unsigned off=k?256:8,n=k?9:3;const uint32_t(*a)[4]=k?raw:main;qj_put(h+off,n,0,2);
  for(unsigned i=0;i<n;i++){uint8_t *p=h+off+2+12*i;qj_put(p,a[i][0],0,2);qj_put(p+2,a[i][1],0,2);qj_put(p+4,a[i][2],0,4);qj_put(p+8,a[i][3],0,4);}
 }qj_put(h+600,124,0,4);qj_put(h+604,92,0,4);qj_put(h+608,11656,0,4);qj_put(h+612,8742,0,4);
}
static void make_raw(MockWork &w,uint32_t id){
 make_work(w,1,id);Shared h=memory(16384);header(ptr(h.value));add(w,h);
 Shared raw=memory(210510336);uint16_t *v=(uint16_t*)ptr(raw.value);for(size_t i=0;i<105255168;i++)v[i]=1234;add(w,raw);
 Shared preview=memory(4096);jpeg(ptr(preview.value),3888,2918);add(w,preview);
}
static void make_jpeg(MockWork &w,uint32_t id){
 make_work(w,4,id);Shared h=memory(2);ptr(h.value)[0]=255;ptr(h.value)[1]=216;add(w,h);
 Shared image=memory(4096);jpeg(ptr(image.value),11656,8742);ptr(image.value)[0]=ptr(image.value)[1]=0;add(w,image);
}
static int64_t convert(MockWork &w,List &out,uintptr_t caller=B_WRITE_CALLER){
 Shared work{&w,nullptr};int64_t bytes=0;qn_prepare(&out.a,&work,&bytes,caller);return bytes;
}
struct Job {MockWork *work;List result;int64_t bytes;};
static DWORD WINAPI background(void *p){Job *j=(Job*)p;j->bytes=convert(*j->work,j->result);return 0;}
int main(){
 base=0;prepare_original=0xdead;metadata_original=0xbeef;vtable[0x68/8]=(uintptr_t)ptr;vtable[0x88/8]=(uintptr_t)len;unsigned cases=0;
 {
  MockWork w{};make_raw(w,1);unsigned before=owners;List out;int64_t bytes=convert(w,out);
  assert(bytes>0&&bytes%4096==0&&out.a.size==3&&owners==before+3);
  Piece *p=(Piece*)out.a.data;assert(len(p[1].memory.value)==52627584);
  assert(((uint16_t*)ptr(p[1].memory.value))[0]==1234&&w.original.a.size==3);
  assert(((Piece*)w.original.a.data)[1].memory.value!=p[1].memory.value);cases++;
  uint8_t content[0xe0]={};*(uint32_t*)(content+0x90)=1;Shared info{};
  qn_fullsize(&info,content,nullptr,1);assert(*(int*)((uint8_t*)info.value+16)==5952&&*(int*)((uint8_t*)info.value+28)==52627584);free(info.value);
  Rect crop=crop_mock(content,nullptr,1);(void)crop;Rect changed=::crop(content,nullptr,1);assert(changed.left==62&&changed.right==5889);cases++;
 }
 assert(!owners);
 {
  MockWork w{};make_jpeg(w,2);List out;int64_t bytes=convert(w,out);assert(bytes==4096&&out.a.size==1);
  unsigned x=0,y=0;assert(qj_stream(ptr(((Piece*)out.a.data)[0].memory.value),4096,&x,&y)&&x==5828&&y==4371);cases++;
  assert(ptr(((Piece*)w.original.a.data)[1].memory.value)[0]==0);cases++;
 }
 assert(!owners);
 /* 上游直接编码的25MP码流：存储只封装，编码器调用次数必须不增加。
  * 原厂输出SOI被分离；恢复和偶数行裁切只能写私有副本。 */
 for(unsigned height=4371;height<=4372;height++){
  MockWork w{};make_jpeg(w,200+height);Piece *pieces=(Piece*)w.original.a.data;
  uint8_t *source=ptr(pieces[1].memory.value);jpeg(source,5828,height);source[0]=source[1]=0;
  unsigned before=encode_calls;List out;int64_t bytes=convert(w,out);
  assert(bytes==4096&&out.a.size==1&&encode_calls==before);
  unsigned x=0,y=0;uint8_t *result=ptr(((Piece*)out.a.data)[0].memory.value);
  assert(qj_stream(result,4096,&x,&y)&&x==5828&&y==4371);
  assert(source[0]==0&&source[1]==0&&qj_u16(source+7,1)==height);cases++;
 }
 assert(!owners);
 {
  MockWork w{};make_jpeg(w,9000);Piece *pieces=(Piece*)w.original.a.data;
  uint8_t *source=ptr(pieces[1].memory.value);jpeg(source,6000,4000);source[0]=source[1]=0;
  unsigned before=encode_calls;List out;
  assert(convert(w,out)==-1&&!out.a.size&&encode_calls==before);cases++;
 }
 assert(!owners);
 for(unsigned fault=0;fault<4;fault++){
  MockWork w{};make_jpeg(w,10+fault);unsigned before=owners;
  fail_alloc=fault==0;fail_encode=fault==1;fail_sync=fault==2;append_after_encode=fault==3;
  List out;
  int64_t bytes=convert(w,out);assert(bytes==-1&&!out.a.size&&owners==before);cases++;
  fail_alloc=fail_encode=fail_sync=fail_append=append_after_encode=0;
 }
 assert(!owners);
 {
  MockWork quarter_work{},native_work{};make_jpeg(quarter_work,100);
  encoding=CreateEvent(nullptr,TRUE,FALSE,nullptr);continue_encoding=CreateEvent(nullptr,TRUE,FALSE,nullptr);
  Job job{&quarter_work,{},0};HANDLE thread=CreateThread(nullptr,0,background,&job,0,nullptr);
  assert(WaitForSingleObject(encoding,10000)==WAIT_OBJECT_0);
  make_jpeg(native_work,101); /* 拍摄下一张时使用新输出设置，旧工作不被切换。 */
  pthread_mutex_lock(&mutex);for(auto &e:exposures)if(e.valid&&e.id==101)e.quarter=0;pthread_mutex_unlock(&mutex);
  List native_result;assert(convert(native_work,native_result)==123&&native_result.a.size==2);cases++;
  qn_mock_selected=0;SetEvent(continue_encoding);assert(WaitForSingleObject(thread,10000)==WAIT_OBJECT_0);CloseHandle(thread);
  assert(job.bytes==4096&&job.result.a.size==1);cases++;
  CloseHandle(encoding);CloseHandle(continue_encoding);encoding=continue_encoding=nullptr;
  List tether;assert(convert(quarter_work,tether,B_WRITE_CALLER+4)==123&&tether.a.size==2);cases++;
 }
 assert(!owners);
 for(unsigned i=0;i<2;i++){
  metadata_small=i;Shared info{};qn_metadata(&info,nullptr,1);
  assert(((int*)((uint8_t*)info.value+16))[0]==(i?5952:11904));
  assert(((int*)((uint8_t*)info.value+16))[1]==(i?4421:8842));free(info.value);cases++;
 }
 /* Exif大小端的主图更新，IFD1缩略图保持不动；APP2对齐不复用。 */
 for(unsigned be=0;be<2;be++){
  uint8_t h[256]={255,216,255,225};memcpy(h+6,"Exif\0\0",6);uint8_t *t=h+12;
  memcpy(t,be?"MM":"II",2);qj_put(t+2,42,be,2);qj_put(t+4,8,be,4);qj_put(t+8,2,be,2);
  for(unsigned j=0;j<2;j++){uint8_t *p=t+10+12*j;qj_put(p,256+j,be,2);qj_put(p+2,4,be,2);qj_put(p+4,1,be,4);qj_put(p+8,j?8742:11656,be,4);}
  qj_put(t+34,50,be,4);qj_put(t+50,1,be,2);qj_put(t+52,256,be,2);qj_put(t+54,4,be,2);qj_put(t+56,1,be,4);qj_put(t+60,3888,be,4);
  size_t used=12+70;qj_put(h+4,used-4,1,2);h[used]=255;h[used+1]=226;qj_put(h+used+2,1000,1,2);
  uint8_t out[256];assert(qj_copy_prefix(h,used+4,out,sizeof out,5828,4371)==used);
  assert(qj_u32(out+12+18,be)==5828&&qj_u32(out+12+30,be)==4371&&qj_u32(out+12+60,be)==3888);cases++;
  /* 官方4.2.0对齐块是COM，不是APP2；可跨越头/负载边界。 */
  h[used+1]=254;assert(qj_copy_prefix(h,used+4,out,sizeof out,5828,4371)==used);cases++;
  qj_put(h+used+2,6,1,2);memset(h+used+4,0,4);
  assert(qj_copy_prefix(h,used+8,out,sizeof out,5828,4371)==used);cases++;
  h[used+1]=225;qj_put(h+used+2,1000,1,2);
  assert(!qj_copy_prefix(h,used+4,out,sizeof out,5828,4371));cases++;

 }
 {
  qn_mock_selected=1;snapshot(60001);qn_mock_selected=0;snapshot(60002);
  QdDecision q{QD_DECISION_MAGIC,60001,0,0};
  assert(qn_decision_reply(&q)&&qd_decision_valid(&q,60001)); /* UI已切换仍用逐拍快照 */
  q.exposure=60002;assert(qn_decision_reply(&q)&&q.known&&!q.quarter);
  q.exposure=60003;assert(qn_decision_reply(&q)&&!q.known&&!q.quarter);
  q.magic=0;assert(!qn_decision_reply(&q));cases++;
 }
 printf("QUARTER_NATIVE_WORKER_RAW_JPEG_OWNERSHIP_METADATA_OVERLAPPING_NATIVE_QUEUE_PASSED cases=%u\n",cases);return 0;
}
