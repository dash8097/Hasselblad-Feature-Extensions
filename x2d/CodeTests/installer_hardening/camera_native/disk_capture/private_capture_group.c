#include "private_capture_group.h"
#include <string.h>
int x2d_capture_group_arm(X2dCaptureGroup *g,uint64_t job,unsigned frames){
 if(!g||g->active||!job||(frames!=4&&frames!=6&&frames!=16))return 0;
 memset(g,0,sizeof *g);g->job=job;g->frames=frames;g->active=1;return 1;
}
int x2d_capture_group_request_issued(X2dCaptureGroup *g,uint64_t job){
 if(!g||!g->active||g->issued||g->failed||g->job!=job)return 0;
 g->issued=1;return 1;
}
int x2d_capture_group_classify(X2dCaptureGroup *g,uint32_t id,
 const X2dCaptureIdentity *identity,unsigned *index){
 if(index)*index=0;
 if(!g||!index||!g->active||!g->issued)return X2D_CAPTURE_NORMAL;
 for(unsigned i=0;i<g->frames;i++)if((g->seen&(1u<<i))&&g->exposure_ids[i]==id){
  *index=i;
  if(g->failed||(identity&&(identity->frames!=g->frames||identity->sequence!=i||identity->drive_mode!=6))){
   g->failed=1;return X2D_CAPTURE_CONFLICT;
  }
  return X2D_CAPTURE_OWN;
 }
 /* 完整快照已确认普通拍摄时，始终交回原厂。 */
 if(identity&&(identity->drive_mode!=6||identity->frames!=g->frames))return X2D_CAPTURE_NORMAL;
 if(!identity||g->failed||identity->sequence>=g->frames||
    (!g->seen&&identity->sequence!=0)||(g->seen&(1u<<identity->sequence))){
  g->failed=1;return X2D_CAPTURE_CONFLICT;
 }
 *index=identity->sequence;g->exposure_ids[*index]=id;g->seen|=1u<<*index;
 return X2D_CAPTURE_OWN;
}
int x2d_capture_group_written(X2dCaptureGroup *g,unsigned index,uint32_t id){
 if(!g||!g->active||g->failed||index>=g->frames||!(g->seen&(1u<<index))||
    g->exposure_ids[index]!=id||(g->written&(1u<<index)))return 0;
 g->written|=1u<<index;return 1;
}
int x2d_capture_group_close(X2dCaptureGroup *g,uint64_t job,int idle,int released){
 if(!g||!g->active||g->job!=job||!idle||!released)return 0;
 if(!g->failed&&g->written!=((1u<<g->frames)-1))return 0;
 memset(g,0,sizeof *g);return 1;
}
