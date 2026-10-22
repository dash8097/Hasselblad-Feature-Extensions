/* Windows 宿主双线程验证；仅测试构建注入复制中途屏障。 */
#include "precapture_ring.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)){fprintf(stderr,"race check line %d: %s\n",__LINE__,#x);exit(1);} } while(0)
static HANDLE entered, proceed;
static int barrier_enabled;
static struct pc_ring ring;
static struct pc_frame frame;
static int push_result;
void pc_test_copy_barrier(void) {
    if(!barrier_enabled)return;
    CHECK(SetEvent(entered));
    CHECK(WaitForSingleObject(proceed,5000)==WAIT_OBJECT_0);
}
static DWORD WINAPI push_thread(LPVOID arg) {
    (void)arg;
    push_result=pc_push_nv12(&ring,&frame,frame.timestamp_ns);
    return 0;
}
int main(void) {
    unsigned char *store=malloc(PC_STORAGE_BYTES), *pixels=malloc(PC_FRAME_BYTES), *out=malloc(PC_FRAME_BYTES);
    CHECK(store && pixels && out);
    memset(pixels,0x66,PC_FRAME_BYTES);
    CHECK(pc_init(&ring,store,PC_STORAGE_BYTES)==PC_OK);
    CHECK(pc_enable(&ring,1,0,1)==PC_OK);
    frame=(struct pc_frame){pixels,pixels+PC_WIDTH*PC_HEIGHT,PC_WIDTH*PC_HEIGHT,PC_WIDTH*PC_HEIGHT/2,
        PC_WIDTH,PC_WIDTH,PC_WIDTH,PC_HEIGHT,0,0,1};
    for(unsigned i=0;i<60;++i){frame.sequence=i;frame.timestamp_ns=(uint64_t)i*16000000;pixels[0]=(unsigned char)i;CHECK(pc_push_nv12(&ring,&frame,frame.timestamp_ns)==PC_OK);}
    entered=CreateEventW(NULL,TRUE,FALSE,NULL);proceed=CreateEventW(NULL,TRUE,FALSE,NULL);
    CHECK(entered && proceed);
    frame.sequence=60;frame.timestamp_ns=960000000;pixels[0]=60;barrier_enabled=1;
    HANDLE thread=CreateThread(NULL,0,push_thread,NULL,0,NULL);CHECK(thread);
    CHECK(WaitForSingleObject(entered,5000)==WAIT_OBJECT_0);
    struct pc_snapshot snapshot; struct pc_meta meta;
    CHECK(pc_request_freeze(&ring,960000001)==PC_OK);
    CHECK(pc_complete_freeze(&ring,&snapshot)==PC_PENDING);
    CHECK(pc_request_freeze(&ring,960000002)==PC_CAPTURE_BUSY);
    CHECK(SetEvent(proceed));
    CHECK(WaitForSingleObject(thread,5000)==WAIT_OBJECT_0);
    CHECK(push_result==PC_OK);
    CHECK(pc_expire(&ring,PC_WINDOW_NS*100)==PC_PENDING);
    frame.sequence=61;frame.timestamp_ns=976000000;
    CHECK(pc_push_nv12(&ring,&frame,frame.timestamp_ns)==PC_PENDING);
    CHECK(pc_complete_freeze(&ring,&snapshot)==PC_OK);
    CHECK(snapshot.count==60 && snapshot.trigger_ns==960000001);
    for(unsigned i=0;i<60;++i){
        CHECK(pc_copy_frozen(&ring,snapshot.token,i,out,PC_FRAME_BYTES,&meta)==PC_OK);
        CHECK(meta.sequence==i+1 && meta.timestamp_ns==(uint64_t)(i+1)*16000000);
        CHECK(out[0]==i+1 && out[PC_FRAME_BYTES-1]==0x66);
    }
    CHECK(pc_release(&ring,snapshot.token,1000000000)==PC_OK);
    CloseHandle(thread);CloseHandle(entered);CloseHandle(proceed);
    free(out);free(pixels);free(store);
    puts("PRECAPTURE_RACE_PASS full_ring_inflight_copy_trigger_expiry_export");
    return 0;
}
