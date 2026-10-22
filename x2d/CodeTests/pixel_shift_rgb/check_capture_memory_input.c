/* 仅合成替身；不取照片、不连接相机、不伪造原厂完成消息。 */
#include <stdio.h>
#include <stdlib.h>
#include "capture_memory_input.h"
#define CHECK(v) do{if(!(v)){fprintf(stderr,"CAPTURE_MEMORY_CHECK_FAILED line=%u\n",__LINE__);exit(1);}}while(0)
typedef struct {CmView view;unsigned held,retains,releases;} Image;
static uint8_t *raw;
static uint8_t jpg[]={255,216,255,192,0,17,8,11,102,15,48,3,1,0x21,0,2,0x11,0,3,0x11,0,
 255,218,0,12,3,1,0,2,0,3,0,0,63,0,42,255,0,42,255,217};
static int retain(void *context,const X2dQtShared *from,X2dQtShared *out){
 (void)context;Image *i=from->value;CHECK(i&&from->control==i&&!out->value&&!out->control);
 i->held++;i->retains++;*out=*from;return 1;
}
static void release(void *context,X2dQtShared *ref){
 (void)context;Image *i=ref->value;CHECK(i&&ref->control==i&&i->held);i->held--;i->releases++;memset(ref,0,sizeof *ref);
}
static int view(void *context,const X2dQtShared *ref,CmView *out){
 (void)context;Image *i=ref->value;CHECK(i->held);*out=i->view;return 1;
}
static void init(CmInput *p,Image images[7]){
 memset(images,0,7*sizeof *images);
 for(unsigned i=0;i<6;i++)images[i].view=(CmView){11836,8842,23808,1,raw,210510336,0};
 images[6].view=(CmView){3888,2918,0,8,jpg,sizeof jpg,0};
 CHECK(cm_init(p,9,(CmApi){retain,release,view,NULL,NULL}));
 CHECK(x2d_capture_group_request_issued(&p->group,9));
}
static int notify(CmInput *p,Image *image,unsigned index,unsigned kind){
 X2dCaptureIdentity id={6,index,6,0};X2dExposureInfo info={.exposure_id=100+index,.kind=(uint8_t)kind,.image={image,image}};
 return cm_receive(p,&info,&id,1);
}
static void first_preview_and_slices(void){
 CmInput p;Image images[7];init(&p,images);CmSlice preview,slice[6];
 CHECK(notify(&p,images+6,0,0)==X2D_CAPTURE_OWN);
 CHECK(p.group.seen==1&&cm_preview_take(&p,&preview));
 CHECK(preview.bytes==sizeof jpg&&!memcmp(preview.data,jpg,sizeof jpg));
 CHECK(!cm_preview_done(&p,10,preview.ticket,1));CHECK(images[6].held);
 CHECK(cm_preview_done(&p,9,preview.ticket,1));CHECK(!images[6].held&&p.preview_copied);
 CHECK(!cm_preview_take(&p,&preview));
 for(unsigned i=0;i<6;i++)CHECK(notify(&p,images+i,i,1)==X2D_CAPTURE_OWN);
 CHECK(!cm_consumed(&p));
 for(unsigned row=0;row<8842;row+=1024){
  unsigned rows=8842-row<1024?8842-row:1024;
  for(unsigned i=0;i<6;i++){
   CHECK(cm_rows_take(&p,i,1024,slice+i));CHECK(slice[i].first_row==row&&slice[i].rows==rows);
   CHECK(slice[i].data==raw+(size_t)row*23808&&slice[i].bytes==(size_t)rows*23808);
   CHECK(!cm_rows_take(&p,i,1024,&preview));CHECK(images[i].held==1);
  }
  for(unsigned i=6;i-->0;){
   CHECK(!cm_rows_done(&p,i,9,slice[i].ticket+100,1));
   CHECK(cm_rows_done(&p,i,9,slice[i].ticket,1));
   CHECK(images[i].held==(row+rows==8842?0u:1u));
  }
 }
 CHECK(cm_consumed(&p)&&cm_drained(&p));
 for(unsigned i=0;i<7;i++)CHECK(images[i].retains==1&&images[i].releases==1&&!images[i].held);
}
static void cancel_leased_rows_and_preview(void){
 CmInput p;Image images[7];init(&p,images);CmSlice a,b;
 CHECK(notify(&p,images+6,0,0)==X2D_CAPTURE_OWN);CHECK(cm_preview_take(&p,&b));
 for(unsigned i=0;i<6;i++)CHECK(notify(&p,images+i,i,1)==X2D_CAPTURE_OWN);
 CHECK(cm_rows_take(&p,0,1024,&a));cm_abort(&p);
 CHECK(!cm_drained(&p)&&images[0].held&&images[6].held);
 for(unsigned i=1;i<6;i++)CHECK(!images[i].held&&images[i].releases==1);
 CHECK(!cm_rows_take(&p,1,1024,&a));CHECK(cm_rows_done(&p,0,9,a.ticket,1));
 CHECK(!cm_drained(&p));CHECK(cm_preview_done(&p,9,b.ticket,1));
 CHECK(cm_drained(&p)&&!cm_consumed(&p)&&!p.preview_copied);
}
static void reject_bad_inputs(void){
 CmInput p;Image images[7];init(&p,images);
 X2dCaptureIdentity normal={1,0,0,0};X2dExposureInfo info={.exposure_id=999,.kind=1,.image={images,images}};
 CHECK(cm_receive(&p,&info,&normal,1)==X2D_CAPTURE_NORMAL&&!p.failed&&!images[0].held);
 images[0].view.stride=23800;CHECK(notify(&p,images,0,1)==X2D_CAPTURE_CONFLICT);
 CHECK(p.failed&&cm_drained(&p)&&images[0].retains==images[0].releases);
 init(&p,images);CHECK(notify(&p,images+1,1,1)==X2D_CAPTURE_CONFLICT&&p.failed);
 init(&p,images);CHECK(notify(&p,images,0,1)==X2D_CAPTURE_OWN);
 CHECK(notify(&p,images,0,1)==X2D_CAPTURE_CONFLICT&&cm_drained(&p));
 init(&p,images);images[6].view.bytes--;
 CHECK(notify(&p,images+6,0,0)==X2D_CAPTURE_CONFLICT&&cm_drained(&p));
 init(&p,images);info=(X2dExposureInfo){.exposure_id=100,.kind=0,.image={images+6,images+6}};
 X2dCaptureIdentity first={6,0,6,0};
 CHECK(cm_receive(&p,&info,&first,0)==X2D_CAPTURE_CONFLICT&&cm_drained(&p));
}
static void merge_group_lease(void){
 for(unsigned cancel=0;cancel<2;cancel++){
  CmInput p;Image images[7];init(&p,images);CmView views[6];uint64_t ticket;
  CHECK(!cm_merge_take(&p,views,&ticket));
  CHECK(notify(&p,images+6,0,0)==X2D_CAPTURE_OWN);
  CmSlice preview;CHECK(cm_preview_take(&p,&preview)&&cm_preview_done(&p,9,preview.ticket,1));
  for(unsigned i=0;i<6;i++)CHECK(notify(&p,images+i,i,1)==X2D_CAPTURE_OWN);
  CHECK(cm_merge_take(&p,views,&ticket));
  CHECK(!cm_merge_take(&p,views,&ticket)&&!cm_rows_take(&p,0,1024,&preview));
  if(cancel)cm_abort(&p);
  for(unsigned i=0;i<6;i++)CHECK(images[i].held==1&&views[i].data==raw);
  CHECK(!cm_merge_done(&p,10,ticket,1)&&!cm_merge_done(&p,9,ticket+1,1));
  CHECK(cm_merge_done(&p,9,ticket,1)&&cm_drained(&p));
  CHECK(cm_consumed(&p)==!cancel&&p.group.written==0);
  CHECK(!cm_merge_done(&p,9,ticket,1));
  for(unsigned i=0;i<7;i++)CHECK(images[i].retains==images[i].releases&&!images[i].held);
 }
}
int main(void){
 raw=malloc(210510336);CHECK(raw); /* 六个替身仅借用同一测试平面，绝不模拟容量实测。 */
 first_preview_and_slices();cancel_leased_rows_and_preview();reject_bad_inputs();merge_group_lease();free(raw);
 puts("FIRST_FRAME_PREVIEW_AND_SIX_RAW_MEMORY_LEASES_RELEASE_ORDER_PASSED_OFFLINE_ONLY");return 0;
}
