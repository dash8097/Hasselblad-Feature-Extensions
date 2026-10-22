#include "precapture_ring.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"check failed line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
int main(void) {
    struct pc_ring r;
    unsigned char *arena=malloc(PC_STORAGE_BYTES+32),*y=malloc(1616*PC_HEIGHT),*uv=malloc(1632*PC_HEIGHT/2),*out=malloc(PC_FRAME_BYTES);
    CHECK(arena && y && uv && out);
    memset(arena,0xCC,16);memset(arena+PC_STORAGE_BYTES+16,0xDD,16);
    memset(y,0x11,1616*PC_HEIGHT);memset(uv,0x22,1632*PC_HEIGHT/2);
    struct pc_frame f={y,uv,1616*PC_HEIGHT,1632*PC_HEIGHT/2,1616,1632,1600,1200,0,0,7};
    struct pc_snapshot s;struct pc_meta m;
    CHECK(pc_init(&r,arena+16,PC_STORAGE_BYTES-1)==PC_INVALID);
    CHECK(pc_init(&r,arena+16,PC_STORAGE_BYTES)==PC_OK);
    CHECK(pc_push_nv12(&r,&f,0)==PC_DISABLED);
    CHECK(pc_enable(&r,1,0,7)==PC_OK);
    CHECK(pc_freeze(&r,0,&s)==PC_EMPTY);
    CHECK(pc_freeze(&r,0,(struct pc_snapshot *)&r)==PC_INVALID);
    CHECK(pc_freeze(&r,0,(struct pc_snapshot *)(arena+16))==PC_INVALID);
    for(unsigned i=0;i<90;++i){
        f.sequence=i;f.timestamp_ns=(uint64_t)i*16666667; y[0]=(unsigned char)i;
        CHECK(pc_push_nv12(&r,&f,f.timestamp_ns)==PC_OK);
    }
    CHECK(r.count==60);
    CHECK(pc_freeze(&r,f.timestamp_ns+1,&s)==PC_OK && s.count==60);
    CHECK(pc_copy_frozen(&r,s.token,0,out,PC_FRAME_BYTES,&m)==PC_OK && m.sequence==30 && out[0]==30);
    CHECK(out[PC_WIDTH]==0x11 && out[PC_WIDTH*PC_HEIGHT]==0x22 && out[PC_FRAME_BYTES-1]==0x22);
    memset(y,0,1616*PC_HEIGHT);memset(uv,0,1632*PC_HEIGHT/2);
    CHECK(pc_copy_frozen(&r,s.token,59,out,PC_FRAME_BYTES,&m)==PC_OK && out[0]==89 && out[PC_FRAME_BYTES-1]==0x22);
    CHECK(pc_copy_frozen(&r,s.token,60,out,PC_FRAME_BYTES,&m)==PC_STALE);
    CHECK(pc_copy_frozen(&r,s.token,0,out,1,&m)==PC_INVALID);
    CHECK(pc_copy_frozen(&r,s.token,0,out,PC_FRAME_BYTES,(struct pc_meta *)&r)==PC_INVALID);
    CHECK(pc_copy_frozen(&r,s.token,0,out,PC_FRAME_BYTES,(struct pc_meta *)(arena+16))==PC_INVALID);
    CHECK(pc_copy_frozen(&r,s.token,0,out,PC_FRAME_BYTES,(struct pc_meta *)out)==PC_INVALID);
    CHECK(pc_push_nv12(&r,&f,f.timestamp_ns)==PC_FROZEN);
    CHECK(pc_expire(&r,UINT64_C(99999999999))==PC_FROZEN);
    CHECK(pc_freeze(&r,f.timestamp_ns+2,&s)==PC_CAPTURE_BUSY);
    CHECK(pc_release(&r,s.token+1,0)==PC_STALE);
    CHECK(pc_copy_frozen(&r,s.token,59,out,PC_FRAME_BYTES,&m)==PC_OK && m.sequence==89);
    CHECK(pc_enable(&r,0,0,7)==PC_FROZEN);
    CHECK(pc_release(&r,s.token,0)==PC_OK);
    CHECK(pc_copy_frozen(&r,s.token,0,out,PC_FRAME_BYTES,&m)==PC_STALE);
    CHECK(pc_push_nv12(&r,&f,f.timestamp_ns)==PC_DISABLED);
    CHECK(pc_enable(&r,1,0,7)==PC_OK);
    /* 30 fps 最近1秒只有30帧，不从前一秒补60帧。 */
    for(unsigned i=0;i<90;++i){f.sequence=i;f.timestamp_ns=(uint64_t)i*33333334;CHECK(pc_push_nv12(&r,&f,f.timestamp_ns)==PC_OK);}
    CHECK(pc_freeze(&r,f.timestamp_ns+1,&s)==PC_OK && s.count==30);
    CHECK(pc_copy_frozen(&r,s.token,0,out,PC_FRAME_BYTES,&m)==PC_OK && m.sequence==60);
    CHECK(pc_release(&r,s.token,0)==PC_OK);
    /* 精确1秒边界保留；超过1秒无新帧也可淘汰。 */
    f.sequence=1;f.timestamp_ns=0;CHECK(pc_push_nv12(&r,&f,0)==PC_OK);
    CHECK(pc_expire(&r,PC_WINDOW_NS)==PC_OK && r.count==1);
    CHECK(pc_expire(&r,PC_WINDOW_NS+1)==PC_OK && r.count==0);
    CHECK(pc_freeze(&r,PC_WINDOW_NS+2,&s)==PC_EMPTY);
    CHECK(pc_enable(&r,1,100,8)==PC_OK);
    f.timestamp_ns=100;CHECK(pc_push_nv12(&r,&f,100)==PC_STALE);
    f.stream_epoch=8;CHECK(pc_push_nv12(&r,&f,100)==PC_OK);
    CHECK(pc_push_nv12(&r,&f,101)==PC_STALE);
    f.sequence=2;CHECK(pc_push_nv12(&r,&f,101)==PC_STALE);
    f.timestamp_ns=102;CHECK(pc_push_nv12(&r,&f,101)==PC_STALE);
    CHECK(pc_push_nv12(&r,&f,102)==PC_OK);
    CHECK(pc_expire(&r,99)==PC_CLOCK_RESET && r.count==0);
    f.timestamp_ns=99;CHECK(pc_push_nv12(&r,&f,99)==PC_OK);
    f.timestamp_ns=101;f.sequence=3;CHECK(pc_push_nv12(&r,&f,101)==PC_OK);
    /* 延迟到达不能保证保留原始窗口，明确拒绝，不伪报完整。 */
    CHECK(pc_freeze(&r,101,&s)==PC_STALE);
    CHECK(pc_freeze(&r,102,&s)==PC_OK && s.count==2);
    CHECK(pc_copy_frozen(&r,s.token,0,out,PC_FRAME_BYTES,&m)==PC_OK && m.timestamp_ns==99);
    CHECK(pc_release(&r,s.token,102)==PC_OK);
    f.y_stride=SIZE_MAX;CHECK(pc_push_nv12(&r,&f,101)==PC_INVALID);f.y_stride=1616;
    f.width=800;CHECK(pc_push_nv12(&r,&f,101)==PC_INVALID);f.width=1600;
    f.uv_bytes=1;CHECK(pc_push_nv12(&r,&f,101)==PC_INVALID);f.uv_bytes=1632*PC_HEIGHT/2;
    f.timestamp_ns=1;CHECK(pc_push_nv12(&r,&f,PC_WINDOW_NS+2)==PC_STALE);
    /* 锁竞争必须立即返回，不能阻塞原厂线程或重复冻结。 */
    atomic_flag_test_and_set(&r.lock);
    CHECK(pc_push_nv12(&r,&f,PC_WINDOW_NS+2)==PC_BUSY);
    CHECK(pc_freeze(&r,PC_WINDOW_NS+3,&s)==PC_PENDING);
    CHECK(pc_expire(&r,PC_WINDOW_NS+2)==PC_BUSY);
    CHECK(pc_enable(&r,0,PC_WINDOW_NS+2,8)==PC_BUSY);
    atomic_flag_clear(&r.lock);
    CHECK(pc_push_nv12(&r,&f,PC_WINDOW_NS+4)==PC_PENDING);
    CHECK(pc_expire(&r,PC_WINDOW_NS*9)==PC_PENDING);
    CHECK(pc_enable(&r,0,PC_WINDOW_NS*9,9)==PC_PENDING);
    CHECK(pc_complete_freeze(&r,&s)==PC_EMPTY);
    /* 注册快门后即使后台稍后完成，也不得用轮询时间淘汰原始窗口。 */
    CHECK(pc_enable(&r,1,0,8)==PC_OK);
    f.timestamp_ns=10;f.sequence=1;
    CHECK(pc_push_nv12(&r,&f,10)==PC_OK);
    atomic_flag_test_and_set(&r.lock);
    CHECK(pc_request_freeze(&r,11)==PC_OK);
    CHECK(pc_request_freeze(&r,12)==PC_CAPTURE_BUSY);
    CHECK(pc_complete_freeze(&r,&s)==PC_PENDING);
    atomic_flag_clear(&r.lock);
    CHECK(pc_expire(&r,PC_WINDOW_NS*100)==PC_PENDING);
    CHECK(pc_complete_freeze(&r,&s)==PC_OK && s.count==1 && s.trigger_ns==11);
    CHECK(pc_copy_frozen(&r,s.token,0,out,PC_FRAME_BYTES,&m)==PC_OK && m.timestamp_ns==10);
    CHECK(pc_release(&r,s.token,20)==PC_OK);
    CHECK(pc_complete_freeze(&r,&s)==PC_EMPTY);
    CHECK(pc_request_freeze(&r,UINT64_MAX)==PC_INVALID);
    for(unsigned i=0;i<16;++i)CHECK(arena[i]==0xCC && arena[PC_STORAGE_BYTES+16+i]==0xDD);
    printf("PRECAPTURE_RING_PASS checks=%u frame_bytes=%zu capacity_bytes=%zu\n",checks,PC_FRAME_BYTES,PC_STORAGE_BYTES);
    free(out);free(uv);free(y);free(arena);return 0;
}
