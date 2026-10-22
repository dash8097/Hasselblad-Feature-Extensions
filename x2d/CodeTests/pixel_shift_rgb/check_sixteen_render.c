/* 自造 metadata/路由：验证逐块编码顺序、生命周期和失败保留，无相机访问。 */
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "factory_native_3fr.h"
/* 相机 AArch64 的 long 是64位；Windows LLP64 用固定宽度模拟系统调用。 */
#define long int64_t
#define FACTORY_SIXTEEN_HOST_CHECK 1
#define FACTORY_TASK_ION_FLAGS 16
#define FACTORY_SYNC_DEVICE 2
#define FACTORY_JPEG_REFERENCE_SHARPEN 1
#define FS_AMOUNT 600u
#define FS_RADIUS_TENTHS 18u
#define CBW_METADATA_MAX (16u*1024u*1024u)
typedef struct {unsigned width,height,stride_pixels,allocation_bytes,content_bytes;struct {unsigned offset;} planes[1];} FactoryBufferGeometry;
typedef struct {uint64_t opaque[16];} Iter;
struct module{const char*name;int(*attach)(const char*,void**);int(*detach)(void*);void*handle;};
typedef struct {int kind,active;uint8_t map[4096];} Buffer;
static Buffer buffers[2];static uint8_t header[16384],metadata[120];
static unsigned rendered,encoded,reads,peak,active,stops,clears,allocations,sharpened;
static int native_active,fail_rpc=-1,fail_encode=-1,fail_allocate=-1,existing=-1,fail_stop;
static char progress_line[32];
static const char *job="/mnt/media_rw/ssd/multishot/native-album-ui-abcdef01-1";
static void text(const char *s){(void)s;}static void number(unsigned n){(void)n;}
static int capture_runtime_job(char out[108],const char *s){if(strcmp(s,job))return 0;strcpy(out,s);return 1;}
static int stack_path(char out[192],const char *s,const char *leaf){return snprintf(out,192,"%s/%s",s,leaf)>0;}
static long sc(long nr,long a,long b,long c,long d){
 (void)c;(void)d;
 if(nr==56){const char *p=(void*)b;char path[192];
  stack_path(path,job,"sixteen.progress.tmp");if(!strcmp(p,path))return 103;
  stack_path(path,job,"rawpb.bin");if(!strcmp(p,path))return 100;
  stack_path(path,job,"merged.3FR");if(!strcmp(p,path))return 101;
  unsigned index=999;int used=0;char prefix[192];snprintf(prefix,sizeof prefix,"%s/render-tile-",job);
  assert(!strncmp(p,prefix,strlen(prefix)));
  assert(sscanf(p+strlen(prefix),"%u.jpg%n",&index,&used)==1&&!p[strlen(prefix)+used]&&index<25);
  return (int)index==existing?102:-2;
 }
 if(nr==62)return a==100?120:INT64_C(3265286144);
 if(nr==64){assert(a==103&&c>0&&c<(int)sizeof progress_line);memcpy(progress_line,(void*)b,(size_t)c);progress_line[c]=0;return c;}
 if(nr==38){assert(a==-100&&c==-100);return 0;}
 if(nr==57||nr==215||nr==101)return 0;
 assert(0);return -1;
}
static long jpeg_sc(long nr,long a,long b,long c,long d,long e,long f){
 (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;assert(nr==222);return (long)metadata;
}
static int stack_read(int fd,void *out,uint32_t bytes,uint64_t offset){
 if(fd==100){assert(bytes==120&&!offset);memset(out,1,bytes);return 1;}
 assert(fd==101);
 if(!offset){assert(bytes==16384);memcpy(out,header,bytes);}
 else{assert(offset>=16384&&offset+bytes<=INT64_C(3265283168));memset(out,0,bytes);reads++;}
 return 1;
}
static int crm_rawpb_wb_valid(const void *m,size_t bytes){assert(m==metadata&&bytes==120);return 1;}
static void *memory_request_template(const void *m,unsigned n){assert(m==metadata&&n==120);return (void*)11;}
static int factory_raw_tile(int(*reader)(void*,uint64_t,unsigned char*,uint32_t),void *context,uint16_t *target,
 unsigned sw,unsigned sh,unsigned source_stride,unsigned tw,unsigned th,unsigned stride,unsigned ox,unsigned oy,
 unsigned cx,unsigned cy,uint16_t *a,uint16_t *b){
 (void)a;(void)b;(void)stride;assert(target&&sw==46652&&sh==34996&&source_stride==46652);
 assert(tw==11836&&th==8842&&cx==128&&cy==96&&ox==(rendered%5)*11648&&oy==(rendered/5)*8736);
 assert(rendered==encoded);return reader(context,((uint64_t)oy*sw+ox)*2,(void*)target,4);
}
static int duss_hal_attach_ion_mem(const char *s,void **p){(void)s;(void)p;return 0;}
static int duss_hal_detach_ion_mem(void *p){(void)p;return 0;}
static int duss_hal_attach_verislcon_ienc(const char *s,void **p){(void)s;(void)p;return 0;}
static int duss_hal_detach_verislcon_ienc(void *p){(void)p;return 0;}
static void *dbus_bus_get(int k,void *p){assert(k==1&&!p);return (void*)1;}
static int factory_native_group(void *b){assert(b==(void*)1);return 255;}
static int duss_hal_initialize(void *m){assert(m);return 0;}
static int duss_hal_deinitialize(void){assert(!active);return 0;}
static int duss_hal_device_open(const char *s,void *c,void **p){assert(!strcmp(s,"/dev/ion")&&c);*p=(void*)2;return 0;}
static int duss_hal_device_start(void *p,void *c){assert(p==(void*)2&&c);return 0;}
static int duss_hal_device_stop(void *p,void *c){assert(p==(void*)2&&!c&&!active);return 0;}
static int duss_hal_device_close(void *p){assert(p==(void*)2&&!active);return 0;}
static int factory_buffer_geometry(FactoryBufferGeometry *g,unsigned w,unsigned h,unsigned format){
 *g=(FactoryBufferGeometry){w,h,11904,4096,512,{{0}}};assert(format==1||format==6);return 1;
}
static int factory_buffer_fits(const FactoryBufferGeometry *g,unsigned n){return g->allocation_bytes==n;}
static int duss_hal_mem_alloc(void *ion,void **out,unsigned size,unsigned align,unsigned pool,unsigned flags){
 assert(ion==(void*)2&&size==4096&&align==1024&&pool==23&&flags==16);
 if((int)allocations++==fail_allocate)return 1;
 unsigned slot=buffers[0].active?1:0;assert(!buffers[slot].active);
 buffers[slot].active=1;buffers[slot].kind=(int)slot;*out=buffers+slot;active++;if(active>peak)peak=active;return 0;
}
static int duss_hal_mem_get_size(void *p,unsigned *n){assert(((Buffer*)p)->active);*n=4096;return 0;}
static int duss_hal_mem_map(void *p,void **m){assert(((Buffer*)p)->active);*m=((Buffer*)p)->map;return 0;}
static int duss_hal_mem_sync(void *p,int direction){assert(((Buffer*)p)->active&&direction==2);return 0;}
static int duss_hal_mem_unmap(void *p){assert(((Buffer*)p)->active);return 0;}
static int duss_hal_mem_free(void *p){Buffer *b=p;assert(b->active&&!native_active);b->active=0;active--;return 0;}
static int duss_hal_mem_share(void *p,uint64_t *n){assert(((Buffer*)p)->active);*n=3+((Buffer*)p)->kind;return 0;}
static int duss_hal_mem_get_phys_addr(void *p,uint64_t *n){return duss_hal_mem_share(p,n);}
static void *rebuild_request_sized(void *event,uint64_t *handles,FactoryBufferGeometry *g,uint64_t *sizes){
 assert(event==(void*)11&&handles[0]==3&&handles[1]==4&&g[0].width==11836&&sizes[0]==4096);return (void*)12;
}
static int dbus_message_set_sender(void *m,const char *s){assert(m==(void*)12&&!s);return 1;}
static int dbus_message_set_destination(void *m,const char *s){assert(m==(void*)12&&!strcmp(s,"com.hasselblad.camera"));return 1;}
static int factory_route_publish(uint64_t *ids){assert(ids[0]==3&&ids[1]==4&&!native_active);native_active=1;return 1;}
static void *dbus_connection_send_with_reply_and_block(void *bus,void *m,int timeout,void *error){
 assert(bus==(void*)1&&m==(void*)12&&timeout==20000&&!error&&rendered==encoded);return (void*)13;
}
static int dbus_message_get_type(void *m){assert(m==(void*)13);return (int)rendered==fail_rpc?3:2;}
static int factory_route_acknowledged(void){assert(native_active);return 1;}
static int dbus_message_iter_init(void *m,Iter *i){assert(m==(void*)13&&i);rendered++;return 0;}
static int factory_stop_native(void *bus){assert(bus==(void*)1);stops++;if(fail_stop&&stops==1)return 0;native_active=0;return 1;}
static int factory_wait_native_idle(void *bus,unsigned n){assert(bus==(void*)1&&n==20);native_active=0;return 1;}
static int factory_route_clear(void){assert(!native_active);clears++;return 1;}
static void dbus_message_unref(void *m){assert(m==(void*)11||m==(void*)12||m==(void*)13);}
static void dbus_connection_unref(void *m){assert(m==(void*)1);}
static uint64_t jpeg_monotonic_ms(void){return 1;}
static void jpeg_elapsed(const char *s,uint64_t a,uint64_t b){(void)s;assert(a==1&&b==1);}
static int factory_reference_sharpen(uint8_t *map,unsigned w,unsigned h,unsigned stride){
 assert(!buffers[0].active&&buffers[1].active&&!native_active&&map==buffers[1].map);
 assert(w==11656&&h==8742&&stride==11904&&rendered==sharpened+1&&sharpened==encoded);
 sharpened++;return 1;
}
static int encode_jpeg_sized_to(void *ion,void *yuv,unsigned w,unsigned h,unsigned stride,const char *path,unsigned cap){
 assert(ion==(void*)2&&yuv==buffers+1&&buffers[1].active&&!buffers[0].active&&!native_active);
 assert(w==11656&&h==8742&&stride==11904&&cap==536870912&&rendered==encoded+1&&sharpened==rendered);
 char expected[192];snprintf(expected,sizeof expected,"%s/render-tile-%u.jpg",job,encoded);assert(!strcmp(path,expected));
 if((int)encoded==fail_encode)return 22;encoded++;return 0;
}
#include "factory_sixteen_disk.inc"
static void reset(void){memset(buffers,0,sizeof buffers);rendered=encoded=reads=peak=active=stops=clears=allocations=sharpened=0;
 native_active=fail_stop=0;fail_rpc=fail_encode=fail_allocate=existing=-1;
 memset(header,0,sizeof header);memcpy(header,"II*\0",4);f3_p32(header+4,8);f3_p16(header+8,1);
 f3_p16(header+10,330);f3_p16(header+12,4);f3_p32(header+14,1);f3_p32(header+18,256);
 const unsigned tags[][2]={{256,46652},{257,34996},{258,16},{259,1},{273,16384},{279,3265266784u},{33422,0x02010100}};
 f3_p16(header+256,7);for(unsigned i=0;i<7;i++){
  uint8_t *p=header+258+i*12;f3_p16(p,tags[i][0]);f3_p16(p+2,i==6?1:4);f3_p32(p+4,i==6?4:1);f3_p32(p+8,tags[i][1]);}
}
int main(void){
 reset();assert(!run_sixteen_disk(job)&&rendered==25&&encoded==25&&reads==25&&peak==2&&!active&&!native_active);
 unsigned phase,r,j;assert(sixteen_progress_parse(progress_line,&phase,&r,&j)&&phase==3&&r==25&&j==25);
 for(unsigned i=0;i<25;i++){
  char line[32];assert(sixteen_progress_line(line,1,i,i));assert(sixteen_progress_parse(line,&phase,&r,&j)&&phase==1&&r==i&&j==i);
  assert(sixteen_progress_line(line,2,i+1,i));assert(sixteen_progress_parse(line,&phase,&r,&j)&&phase==2&&r==i+1&&j==i);
 }
 const char *bad[]={"","RS16 1 26 25\n","RS16 0 0 0\n","RS16 2 2 3\n","RS16 3 25 25\nextra","RS16 2 20 0\n"};
 for(unsigned i=0;i<sizeof bad/sizeof bad[0];i++)assert(!sixteen_progress_parse(bad[i],&phase,&r,&j));
 for(int i=0;i<25;i++){
  reset();fail_rpc=i;assert(run_sixteen_disk(job)&&rendered==(unsigned)i&&encoded==(unsigned)i&&!active&&!native_active);
  reset();fail_encode=i;assert(run_sixteen_disk(job)&&rendered==(unsigned)i+1&&encoded==(unsigned)i&&!active&&!native_active);
  reset();existing=i;assert(run_sixteen_disk(job)&&!rendered&&!encoded&&!active);
 }
 for(int i=0;i<50;i++){reset();fail_allocate=i;assert(run_sixteen_disk(job)&&!active&&!native_active);}
 reset();fail_stop=1;assert(run_sixteen_disk(job)&&!active&&!native_active&&!encoded&&stops==2);
 reset();assert(run_sixteen_disk("/mnt/media_rw/ssd/DCIM")&&!active&&!rendered);
 puts("SIXTEEN_128_RENDER_LIFECYCLE_FAILURES_IMMEDIATE_ENCODING_PRIVATE_MULTISHOT_PASSED");return 0;
}
