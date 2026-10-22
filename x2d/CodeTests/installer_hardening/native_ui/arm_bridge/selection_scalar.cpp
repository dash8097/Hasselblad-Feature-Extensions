#include "../selection_policies.hpp"
#include <cstddef>
extern "C" {
struct X2dRectV1 {double x,y,width,height;int present;};
struct X2dEyeStateV1 {int preferred,chosen;double face_id,missing_since,preferred_since,last_time;};
struct X2dEyeResultV1 {X2dEyeStateV1 state;int eye,fallback_face;};
__attribute__((visibility("default")))int x2d_nearest_eye_v1(const X2dRectV1*left,const X2dRectV1*right,double x,double y,double width,double height){
 if(!left||!right)return -1;
 using x2d::selection::Rect;
 return x2d::selection::nearest_eye(Rect{left->x,left->y,left->width,left->height,left->present!=0},Rect{right->x,right->y,right->width,right->height,right->present!=0},x,y,width,height);
}
__attribute__((visibility("default")))int x2d_eye_step_v1(const X2dEyeStateV1*state,int preferred,double face_id,int left,int right,double now,double delay,X2dEyeResultV1*out){
 if(!state||!out||(left!=0&&left!=1)||(right!=0&&right!=1))return 1;
 using x2d::selection::EyeState;
 auto r=x2d::selection::eye_step(EyeState{state->preferred,state->face_id,state->chosen,state->missing_since,state->preferred_since,state->last_time},preferred,face_id,left!=0,right!=0,now,delay);
 out->state={r.state.preferred,r.state.chosen,r.state.face_id,r.state.missing_since,r.state.preferred_since,r.state.last_time};out->eye=r.eye;out->fallback_face=r.fallback_face?1:0;return 0;
}
}
