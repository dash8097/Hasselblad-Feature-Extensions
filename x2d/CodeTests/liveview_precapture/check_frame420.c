#include "precapture_frame420.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"frame check line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void put32(unsigned char *p,uint32_t n){for(unsigned i=0;i<4;++i)p[i]=(unsigned char)(n>>(i*8));}
int main(int argc,char **argv) {
    CHECK(argc==4);
    unsigned char descriptor[536],*mapped=malloc(8*1024*1024),*store=malloc(PC_STORAGE_BYTES),*out=malloc(PC_FRAME_BYTES);
    CHECK(mapped && store && out);struct pc_ring ring;struct pc_frame frame;
    CHECK(pc_init(&ring,store,PC_STORAGE_BYTES)==PC_OK);
    for(int c=1;c<argc;++c){
        FILE *f=fopen(argv[c],"rb");CHECK(f);CHECK(fread(descriptor,1,536,f)==536);CHECK(fgetc(f)==EOF);fclose(f);
        CHECK(pc_frame420_nv12(descriptor,536,mapped,8*1024*1024,100,1,1,&frame)==PC_OK);
        for(unsigned row=0;row<PC_HEIGHT;++row)memset((void *)(frame.y+row*frame.y_stride),row&255,PC_WIDTH);
        for(unsigned row=0;row<PC_HEIGHT/2;++row)memset((void *)(frame.uv+row*frame.uv_stride),0x77,PC_WIDTH);
        CHECK(pc_enable(&ring,1,0,1)==PC_OK);CHECK(pc_push_nv12(&ring,&frame,100)==PC_OK);
        struct pc_snapshot snapshot;struct pc_meta meta;
        CHECK(pc_freeze(&ring,101,&snapshot)==PC_OK && snapshot.count==1);
        CHECK(pc_copy_frozen(&ring,snapshot.token,0,out,PC_FRAME_BYTES,&meta)==PC_OK);
        CHECK(out[0]==0 && out[PC_WIDTH]==1 && out[(PC_HEIGHT-1)*PC_WIDTH]==((PC_HEIGHT-1)&255));
        CHECK(out[PC_WIDTH*PC_HEIGHT]==0x77 && out[PC_FRAME_BYTES-1]==0x77);
        CHECK(pc_release(&ring,snapshot.token,102)==PC_OK);
        CHECK(pc_frame420_nv12(descriptor,535,mapped,8*1024*1024,100,1,1,&frame)==PC_INVALID);
        CHECK(pc_frame420_nv12(descriptor,536,mapped,1024,100,1,1,&frame)==PC_INVALID);
        unsigned char bad[536];memcpy(bad,descriptor,536);put32(bad+0x54,0);
        CHECK(pc_frame420_nv12(bad,536,mapped,8*1024*1024,100,1,1,&frame)==PC_INVALID);
        memcpy(bad,descriptor,536);put32(bad+0x40,UINT32_MAX);
        CHECK(pc_frame420_nv12(bad,536,mapped,8*1024*1024,100,1,1,&frame)==PC_INVALID);
        memcpy(bad,descriptor,536);put32(bad+0x28,0x4e24);
        CHECK(pc_frame420_nv12(bad,536,mapped,8*1024*1024,100,1,1,&frame)==PC_INVALID);
    }
    free(out);free(store);free(mapped);
    puts("PRECAPTURE_FACTORY_DESCRIPTOR_TO_RING_PASS cases=3 padded_planes_and_invalid_inputs");return 0;
}
