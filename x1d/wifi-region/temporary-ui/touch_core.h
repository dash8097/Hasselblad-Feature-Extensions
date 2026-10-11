#ifndef HBL_TOUCH_CORE_H
#define HBL_TOUCH_CORE_H
#include <math.h>
enum { TB_BEGIN=1,TB_END=2,TB_ZON=4,TB_ZOFF=8,TB_BASE=16,TB_AF=32,TB_ZOOM=64 };
typedef struct {int owner,inside,held;double lastX,lastY,fx,fy;} TouchCore;
typedef struct {int id;double x,y;} TouchPoint;
typedef struct {
 int enabled,focus,zoom,zoomActive;
 double left,top,padWidth,padHeight,width,height,xCount,yCount,marginX,marginY,itemWidth,itemHeight,currentX,currentY;
} TouchFrame;
typedef struct {unsigned effects;double fx,fy,x,y;} TouchResult;
static int touch_inside(const TouchFrame *f,double x,double y) {
 return x>=f->left&&x<f->left+f->padWidth&&y>=f->top&&y<f->top+f->padHeight;
}
static void touch_begin(TouchCore *s,const TouchFrame *f,TouchResult *r) {
 if(f->focus&&!s->held){s->held=1;r->effects|=TB_BEGIN;}
 s->fx=f->currentX;s->fy=f->currentY;
}
static TouchResult touch_dispatch(TouchCore *s,int command,const TouchPoint *points,int count,const TouchFrame *f) {
 TouchResult r={0,s->fx,s->fy,s->lastX,s->lastY};
 if(command==3){
  if(s->held)r.effects|=TB_END;
  s->held=0;s->owner=-1;s->inside=0;return r;
 }
 if(command==1){
  for(int i=0;i<count;i++)if(points[i].id==s->owner){
   if(s->held)r.effects|=TB_END;
   s->held=0;s->owner=-1;s->inside=0;r.effects|=TB_ZOFF;break;
  }
  return r;
 }
 if(command==2&&!f->enabled)return r;
 if(command==0||s->owner<0){
  if(s->owner>=0)return r;
  for(int i=0;i<count;i++)if(touch_inside(f,points[i].x,points[i].y)){
   s->owner=points[i].id;s->inside=1;touch_begin(s,f,&r);
   s->lastX=points[i].x;s->lastY=points[i].y;
   r.effects|=TB_BASE|(f->zoom?TB_ZON:TB_ZOFF);break;
  }
 }else for(int i=0;i<count;i++){
  const TouchPoint p=points[i];if(p.id!=s->owner)continue;
  if(!touch_inside(f,p.x,p.y)){s->inside=0;break;}
  if(!s->inside){touch_begin(s,f,&r);s->lastX=p.x;s->lastY=p.y;s->inside=1;r.effects|=TB_BASE;break;}
  if(f->zoom&&f->zoomActive){r.effects|=TB_ZOOM;r.x=p.x;r.y=p.y;return r;}
  if(f->focus&&!f->zoomActive){
   if(f->padWidth<=0||f->padHeight<=0||f->width<=0||f->height<=0||f->xCount<1||f->yCount<1)break;
   const double dx=p.x-s->lastX,dy=p.y-s->lastY;s->lastX=p.x;s->lastY=p.y;
   if(dx==0&&dy==0)break;
   const double minX=(f->marginX+f->itemWidth/2)/f->width,maxX=(f->marginX+(f->xCount-.5)*f->itemWidth)/f->width;
   const double minY=(f->marginY+f->itemHeight/2)/f->height,maxY=(f->marginY+(f->yCount-.5)*f->itemHeight)/f->height;
   s->fx=fmax(minX,fmin(maxX,s->fx+dx/f->padWidth));s->fy=fmax(minY,fmin(maxY,s->fy+dy/f->padHeight));r.effects|=TB_AF;
  }
  break;
 }
 r.fx=s->fx;r.fy=s->fy;r.x=s->lastX;r.y=s->lastY;return r;
}
#endif
