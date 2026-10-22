#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include "private_capture_group.c"
int main(void){
 X2dCaptureGroup g={0};unsigned index;
 X2dCaptureIdentity ordinary={1,0,0,0},four={4,0,6,0},six={6,0,6,0};
 assert(!x2d_capture_group_arm(&g,0,4)&&!x2d_capture_group_arm(&g,1,5));
 assert(x2d_capture_group_arm(&g,1,4)&&!x2d_capture_group_arm(&g,2,6));
 assert(x2d_capture_group_classify(&g,300,&four,&index)==X2D_CAPTURE_NORMAL);
 assert(!x2d_capture_group_request_issued(&g,2)&&x2d_capture_group_request_issued(&g,1));
 assert(!x2d_capture_group_request_issued(&g,1));
 assert(x2d_capture_group_classify(&g,500,&ordinary,&index)==X2D_CAPTURE_NORMAL&&!g.seen);
 assert(x2d_capture_group_classify(&g,700,&six,&index)==X2D_CAPTURE_NORMAL&&!g.seen);
 assert(x2d_capture_group_classify(&g,42,&four,&index)==X2D_CAPTURE_OWN&&index==0);
 assert(x2d_capture_group_classify(&g,42,0,&index)==X2D_CAPTURE_OWN&&index==0);
 assert(x2d_capture_group_written(&g,0,42)&&!x2d_capture_group_written(&g,0,42));
 /* 实际编号不必相邻；后续消息可按图像处理完成顺序交错。 */
 const uint32_t ids[]={42,105,91,402};
 const unsigned order[]={2,1,3};
 for(unsigned j=0;j<3;j++){
  unsigned i=order[j];four.sequence=i;
  assert(x2d_capture_group_classify(&g,ids[i],&four,&index)==X2D_CAPTURE_OWN&&index==i);
  assert(!x2d_capture_group_written(&g,i,ids[i]+1));
  assert(x2d_capture_group_written(&g,i,ids[i]));
 }
 assert(!x2d_capture_group_close(&g,2,1,1));
 assert(!x2d_capture_group_close(&g,1,0,1)&&!x2d_capture_group_close(&g,1,1,0));
 assert(x2d_capture_group_close(&g,1,1,1));
 assert(x2d_capture_group_arm(&g,2,6)&&x2d_capture_group_request_issued(&g,2));
 six.sequence=1;
 assert(x2d_capture_group_classify(&g,1,&six,&index)==X2D_CAPTURE_CONFLICT&&g.failed);
 assert(!x2d_capture_group_close(&g,2,0,1)&&x2d_capture_group_close(&g,2,1,1));
 assert(x2d_capture_group_arm(&g,3,6)&&x2d_capture_group_request_issued(&g,3));
 six.sequence=0;assert(x2d_capture_group_classify(&g,8,&six,&index)==X2D_CAPTURE_OWN);
 assert(x2d_capture_group_classify(&g,9,&six,&index)==X2D_CAPTURE_CONFLICT);
 assert(x2d_capture_group_classify(&g,8,0,&index)==X2D_CAPTURE_CONFLICT);
 assert(!x2d_capture_group_written(&g,0,8)&&x2d_capture_group_close(&g,3,1,1));
 assert(x2d_capture_group_arm(&g,4,4)&&x2d_capture_group_request_issued(&g,4));
 assert(x2d_capture_group_classify(&g,77,0,&index)==X2D_CAPTURE_CONFLICT);
 assert(x2d_capture_group_close(&g,4,1,1));
 assert(x2d_capture_group_arm(&g,5,16)&&x2d_capture_group_request_issued(&g,5));
 X2dCaptureIdentity sixteen={16,0,6,0};
 for(unsigned i=0;i<16;i++){
  unsigned seq=i?16-i:0;sixteen.sequence=seq;
  assert(x2d_capture_group_classify(&g,5000+seq*17,&sixteen,&index)==X2D_CAPTURE_OWN&&index==seq);
  assert(x2d_capture_group_written(&g,seq,5000+seq*17));
 }
 assert(g.seen==65535&&g.written==65535&&x2d_capture_group_close(&g,5,1,1));
 puts("PRIVATE_CAPTURE_4_6_16_ACTUAL_IDS_NORMAL_FALLBACK_AND_DRAIN_CHECKS_PASSED");
}
