/* 自造输入 + 原生产计算/队列核心。没有假冒相机服务或硬件。 */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <signal.h>
#include "capture_memory_merge.h"
#include "pixel_chunk_fanout.h"
#include "factory_six_tile_plan.h"
#include "raw_stack_core.h"

#define C(x) do { if (!(x)) { printf("X2D_SIM_ASSERT line=%u expression=%s\n",__LINE__,#x); return 1; } } while (0)

/* Bionic 的 opaque 句柄按其公开函数 ABI 使用，不引入 GNU pthread 结构体。 */
extern int pthread_key_create(unsigned *, void (*)(void *));
extern int pthread_key_delete(unsigned);
extern int pthread_setspecific(unsigned, const void *);
extern void *pthread_getspecific(unsigned);
extern int pthread_create(unsigned long *, const void *, void *(*)(void *), void *);
extern int pthread_join(unsigned long, void **);
static unsigned tls_key;
static void *tls_work(void *p) {
    if (pthread_getspecific(tls_key) || pthread_setspecific(tls_key,p)) return 0;
    return pthread_getspecific(tls_key);
}
static int runtime(void) {
    unsigned long threads[2]; unsigned values[3] = {10,20,30};
    C(!pthread_key_create(&tls_key,0)); C(!pthread_setspecific(tls_key,values));
    for (unsigned i=0;i<2;i++) C(!pthread_create(threads+i,0,tls_work,values+i+1));
    for (unsigned i=0;i<2;i++) { void *v=0; C(!pthread_join(threads[i],&v)); C(v==values+i+1); }
    C(pthread_getspecific(tls_key)==values); C(!pthread_key_delete(tls_key));
    unsigned char *p=mmap(0,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    C(p!=MAP_FAILED); p[0]=91; C(!mprotect(p,4096,PROT_READ)); C(p[0]==91);
    C(!mprotect(p,4096,PROT_READ|PROT_WRITE)); p[1]=92; C(!munmap(p,4096));
    int pair[2]; C(!socketpair(AF_UNIX,SOCK_SEQPACKET,0,pair));
    unsigned sent=0x42cafe,got=0; C(write(pair[0],&sent,4)==4); C(read(pair[1],&got,4)==4 && got==sent);
    C(!close(pair[0])&&!close(pair[1]));
    C(access("/dev/ion",F_OK)==-1 && access("/dev/video0",F_OK)==-1);
    puts("X2D_SIM_RUNTIME_PASS real_bionic=1 real_pthread=1 real_syscalls=1"); return 0;
}

static unsigned freed[4];
static void release_slot(void *ctx,void *owner) { (void)ctx; (*(unsigned *)owner)++; }
static int fanout(void) {
    PcFanout p; PcChunk a,b; uint16_t data[4][6]={{0}}; memset(freed,0,sizeof freed);
    C(pc_init(&p,47,3,10,release_slot,0));
    for (unsigned i=0;i<4;i++) C(pc_submit(&p,i*2,2,data[i],12,freed+i));
    C(!pc_submit(&p,8,2,data[0],12,freed)); /* 满槽不能靠新缓冲绕过背压。 */
    C(pc_take(&p,PC_RAW,&a)&&pc_take(&p,PC_RENDER,&b));
    C(!pc_done(&p,PC_RAW,48,a.ticket,1));
    C(pc_done(&p,PC_RAW,47,a.ticket,1)&&!freed[0]);
    C(!pc_done(&p,PC_RAW,47,a.ticket,1));
    C(pc_done(&p,PC_RENDER,47,b.ticket,0)); /* 显影失败，未借出的块也撤回。 */
    C(pc_drained(&p)&&!pc_complete(&p));
    for(unsigned i=0;i<4;i++) C(freed[i]==1);
    C(pc_init(&p,49,3,2,release_slot,0)); memset(freed,0,sizeof freed);
    C(pc_submit(&p,0,2,data[0],12,freed));
    C(pc_take(&p,PC_RAW,&a)&&pc_take(&p,PC_RENDER,&b)); pc_abort(&p);
    C(!freed[0]&&!pc_drained(&p));
    C(pc_done(&p,PC_RAW,49,a.ticket,1)&&!freed[0]);
    C(pc_done(&p,PC_RENDER,49,b.ticket,1)&&freed[0]==1&&pc_drained(&p));
    C(pc_init(&p,50,3,2,release_slot,0)); memset(freed,0,sizeof freed);
    C(pc_submit(&p,0,2,data[0],12,freed));
    C(pc_take(&p,PC_RAW,&a)&&pc_take(&p,PC_RENDER,&b));
    C(pc_done(&p,PC_RENDER,50,b.ticket,1)&&!freed[0]&&!pc_complete(&p));
    C(pc_done(&p,PC_RAW,50,a.ticket,1)&&freed[0]==1&&pc_complete(&p));
    return 0;
}
static int capture_and_tiles(void) {
    X2dCaptureGroup g={0}; unsigned index;
    C(x2d_capture_group_arm(&g,51,6)); C(!x2d_capture_group_arm(&g,52,6));
    C(x2d_capture_group_request_issued(&g,51)); C(!x2d_capture_group_request_issued(&g,51));
    X2dCaptureIdentity id={.frames=6,.sequence=0,.drive_mode=6};
    C(x2d_capture_group_classify(&g,200,&id,&index)==X2D_CAPTURE_OWN&&index==0);
    C(x2d_capture_group_classify(&g,200,0,&index)==X2D_CAPTURE_OWN);
    C(x2d_capture_group_written(&g,0,200)); C(!x2d_capture_group_written(&g,0,200));
    /* 另一组再次从第0帧出现：拒收，不占用第二份六帧。 */
    C(x2d_capture_group_classify(&g,300,&id,&index)==X2D_CAPTURE_CONFLICT);
    C(!x2d_capture_group_close(&g,51,0,1)); C(!x2d_capture_group_close(&g,51,1,0));
    C(x2d_capture_group_close(&g,51,1,1));
    C(x2d_capture_group_arm(&g,52,6)&&x2d_capture_group_request_issued(&g,52));
    for(unsigned i=0;i<6;i++) {
        id.sequence=i; C(x2d_capture_group_classify(&g,400+i,&id,&index)==X2D_CAPTURE_OWN&&index==i);
        C(x2d_capture_group_written(&g,i,400+i));
    }
    C(x2d_capture_group_close(&g,52,1,1));
    FactorySixTile tiles[6]; uint64_t pixels=0;
    for(unsigned i=0;i<6;i++) {
        C(factory_six_tile(i,tiles+i)); FactorySixTile *t=tiles+i;
        C(t->x+t->width<=23326 && t->y+t->height<=17498 && !(t->x&1) && !(t->y&1));
        C(factory_buffer_fits(&t->raw,210513920)&&factory_buffer_fits(&t->yuv,205893632));
        pixels+=(uint64_t)t->width*t->height;
        for(unsigned j=0;j<i;j++) { FactorySixTile *a=tiles+j;
            C(a->x+a->width<=t->x||t->x+t->width<=a->x||a->y+a->height<=t->y||t->y+t->height<=a->y); }
    }
    C(pixels==UINT64_C(23326)*17498); C(!factory_six_tile(6,tiles));
    return 0;
}
static int exact_file(const char *path,const void *data,size_t bytes) {
    int fd=open(path,O_RDONLY); if(fd<0)return 0;
    unsigned char chunk[4096]; const unsigned char *p=data; int ok=1;
    while(bytes) { size_t n=bytes<sizeof chunk?bytes:sizeof chunk; ssize_t got=read(fd,chunk,n);
        if(got!=(ssize_t)n||memcmp(p,chunk,n)){ok=0;break;} p+=n;bytes-=n; }
    if(ok&&read(fd,chunk,1)!=0)ok=0; if(close(fd))ok=0; return ok;
}
static int full_four_hundred(void) {
    const unsigned width=11663,height=8749,batch=32,max_span=batch+4;
    size_t plane_bytes=(size_t)max_span*23808,output_bytes=(size_t)width*batch*8;
    CmView frames[6];
    for(unsigned i=0;i<6;i++) {
        uint16_t *v=malloc(plane_bytes); C(v);
        frames[i]=(CmView){11836,8842,23808,1,(const uint8_t *)v,plane_bytes,0};
    }
    uint16_t *output=malloc(output_bytes+64); size_t sz=cm_merge_scratch_bytes(width);
    unsigned char *scratch=malloc(sz+64); C(output&&scratch);
    memset(output,0xa5,output_bytes+64); memset(scratch,0xa5,sz+64);
    uint64_t samples=0; unsigned bands=0;
    for(unsigned first=0;first<height;first+=batch) {
        unsigned rows=height-first<batch?height-first:batch,row0=first?first+91:92;
        unsigned last=(first+rows<height?first+rows:height-1)+93,span=last-row0+1;
        C(span<=max_span);
        /* 均匀彩色平面：独立解析解为 GRBG=2000/1000/3000/2000。
         * 空间变化/不同帧差异另由上面的独立参考23行覆盖。 */
        for(unsigned i=0;i<6;i++) {
            uint16_t *v=(uint16_t *)frames[i].data;
            for(unsigned y=0;y<span;y++)for(unsigned x=0;x<11904;x++)
                v[(size_t)y*11904+x]=((row0+y)&1)?((x&1)?3000:2000):((x&1)?2000:1000);
            frames[i].bytes=(size_t)span*23808;
        }
        C(cm_merge_cached_rows(frames,row0,span,width,height,first,rows,output,(size_t)width*rows*8,scratch,sz));
        for(unsigned y=0;y<2*rows;y++)for(unsigned x=0;x<2*width;x++) {
            unsigned expected=(y&1)?((x&1)?2000:3000):((x&1)?1000:2000);
            C(output[(size_t)y*2*width+x]==expected);
        }
        samples+=(uint64_t)width*rows*4; bands++;
    }
    C(samples==UINT64_C(23326)*17498);
    for(unsigned i=0;i<64;i++)C(((unsigned char *)output)[output_bytes+i]==0xa5&&scratch[sz+i]==0xa5);
    for(unsigned i=0;i<6;i++)free((void *)frames[i].data);free(output);free(scratch);
    printf("X2D_SIM_400MP_FULL_CORE_PASS output_samples=%llu bands=%u full_frame_allocation=0 hardware_render=0\n",
           (unsigned long long)samples,bands); return 0;
}
static int four_hundred(void) {
    const unsigned width=11663,height=8749;
    const unsigned firsts[]={0,1017,8746},counts[]={3,17,3};
    unsigned long long checked=0;
    for(unsigned test=0;test<3;test++) {
        unsigned first=firsts[test],rows=counts[test],row0=first?first+91:92;
        unsigned last=(first+rows<height?first+rows:height-1)+93,span=last-row0+1;
        size_t input_bytes=(size_t)span*23808,bytes=(size_t)width*rows*8,sz=cm_merge_scratch_bytes(width);
        CmView frames[6];
        for(unsigned i=0;i<6;i++) {
            uint16_t *v=malloc(input_bytes); C(v);
            for(unsigned y=0;y<span;y++)for(unsigned x=0;x<11904;x++)
                v[(size_t)y*11904+x]=(uint16_t)(x*251+(row0+y)*17+i*10007);
            frames[i]=(CmView){11836,8842,23808,1,(const uint8_t *)v,input_bytes,0};
        }
        uint16_t *out=malloc(bytes+64); unsigned char *scratch=malloc(sz+64); C(out&&scratch);
        memset(out,0xa5,bytes+64); memset(scratch,0xa5,sz+64);
        C(cm_merge_cached_rows(frames,row0,span,width,height,first,rows,out,bytes,scratch,sz));
        char path[64]; snprintf(path,sizeof path,"/fixtures/400-%u.bin",test);
        C(exact_file(path,out,bytes)); checked+=bytes/2;
        for(unsigned i=0;i<64;i++)C(((unsigned char *)out)[bytes+i]==0xa5&&scratch[sz+i]==0xa5);
        /* 短源窗口、过小 scratch 在任何写入前拒绝。 */
        out[0]=0xa5a5;
        C(!cm_merge_cached_rows(frames,row0,span-1,width,height,first,rows,out,bytes,scratch,sz));
        C(out[0]==0xa5a5);
        C(!cm_merge_cached_rows(frames,row0,span,width,height,first,rows,out,bytes,scratch,sz-1));
        C(out[0]==0xa5a5);
        for(unsigned i=0;i<6;i++)free((void *)frames[i].data); free(out); free(scratch);
    }
    C(!fanout()&&!capture_and_tiles()&&!full_four_hundred());
    printf("X2D_SIM_400MP_PASS full_width=23326 full_height=17498 tested_source_rows=23 checked_samples=%llu entire_image=0 hardware_render=0\n",checked);
    return 0;
}
static int stack_case(RsMethod method,unsigned noise) {
    enum { N=19, FRAMES=10003 }; RsConfig cfg={17,1,N,method,noise};
    size_t bytes; C(rs_workspace_bytes(cfg,&bytes)); void *mem=malloc(bytes); C(mem);
    RsCore core; C(rs_init(&core,cfg,mem,bytes));
    uint16_t src[N],expected[N]={0}; uint64_t total[N]={0}; unsigned group[N]={0},pending=0,groups=0;
    for(unsigned f=0;f<FRAMES;f++) {
        for(unsigned i=0;i<N;i++)src[i]=(uint16_t)(i*8191+f*21337+(f%3)*1023);
        C(rs_begin_frame(&core,sizeof src)); C(rs_accumulate_rows(&core,src,0,1)); C(rs_commit_frame(&core));
        for(unsigned i=0;i<N;i++) {
            if(method==RS_AVERAGE) total[i]+=src[i];
            else if(noise) group[i]+=src[i];
            else if(!f||(method==RS_MAXIMUM?src[i]>expected[i]:src[i]<expected[i]))expected[i]=src[i];
        }
        if(noise&&++pending==noise) {
            for(unsigned i=0;i<N;i++) { uint16_t v=(uint16_t)((group[i]+pending/2)/pending);
                if(!groups||(method==RS_MAXIMUM?v>expected[i]:v<expected[i]))expected[i]=v; group[i]=0; }
            groups++; pending=0;
        }
        memset(src,0xa5,sizeof src); /* 已读完后归还并覆盖，不保留上万份原帧。 */
    }
    for(unsigned i=0;i<N;i++) {
        if(method==RS_AVERAGE)expected[i]=(uint16_t)((total[i]+FRAMES/2)/FRAMES);
        else if(noise&&pending) { uint16_t v=(uint16_t)((group[i]+pending/2)/pending);
            if(!groups||(method==RS_MAXIMUM?v>expected[i]:v<expected[i]))expected[i]=v; }
    }
    C(rs_begin_finish(&core)&&rs_normalize_rows(&core,0,1));
    const uint16_t *result=rs_compact(&core,&bytes); C(result&&bytes==sizeof expected&&!memcmp(result,expected,bytes));
    C(!rs_begin_frame(&core,sizeof src)); free(mem); return 0;
}
static int stack(void) {
    for(unsigned mode=0;mode<3;mode++) { C(!stack_case((RsMethod)mode,0)); if(mode!=RS_AVERAGE)C(!stack_case((RsMethod)mode,4)); }
    RsCore core; _Alignas(8) uint32_t accumulator=0; uint16_t v=65535;
    C(rs_init(&core,(RsConfig){1,1,1,RS_AVERAGE,0},&accumulator,sizeof accumulator));
    for(unsigned i=0;i<65535;i++)C(rs_begin_frame(&core,2)&&rs_accumulate_rows(&core,&v,0,1)&&rs_commit_frame(&core));
    C(!rs_begin_frame(&core,2)); C(rs_begin_finish(&core)&&rs_normalize_rows(&core,0,1));
    size_t bytes; const uint16_t *result=rs_compact(&core,&bytes); C(result&&bytes==2&&*result==65535);
    puts("X2D_SIM_STACK_PASS modes=5 small_frames=10003 limit_frames=65535 full_sensor=0");return 0;
}
int main(int argc,char **argv) {
    if(argc!=2)return 64;
    if(!strcmp(argv[1],"runtime"))return runtime();
    if(!strcmp(argv[1],"400mp"))return four_hundred();
    if(!strcmp(argv[1],"stack"))return stack();
    if(!strcmp(argv[1],"expected-failure"))return 19;
    if(!strcmp(argv[1],"expected-crash")){raise(SIGSEGV);return 65;}
    if(!strcmp(argv[1],"expected-timeout")){for(;;)pause();}
    return 64;
}
