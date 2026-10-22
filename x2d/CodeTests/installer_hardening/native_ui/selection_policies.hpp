#pragma once
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>
// H3 / X2D 4.2.0 两个纯策略 JS 的 typed C++ 等价候选。
// 只选择输入中的 ID/坐标，不读取帧、不请求对焦，也不保存设备信息。
namespace x2d::selection {
struct Rect {double x=0,y=0,width=0,height=0;bool present=true;};
struct Face {double id=-1;Rect rect;};
inline bool finite_rect(const Rect&r){return r.present&&std::isfinite(r.x)&&std::isfinite(r.y)&&std::isfinite(r.width)&&std::isfinite(r.height)&&r.width>0&&r.height>0;}
inline double nearest_face(const std::vector<Face>&faces,double x,double y){
    double result=-1,best=std::numeric_limits<double>::infinity();
    if(!std::isfinite(x)||!std::isfinite(y))return result;
    for(const auto&f:faces){const auto&r=f.rect;if(!std::isfinite(f.id)||f.id<0||f.id>=65535||!finite_rect(r))continue;
        const double dx=std::max({r.x-x,0.0,x-r.x-r.width}),dy=std::max({r.y-y,0.0,y-r.y-r.height});const double distance=dx*dx+dy*dy;
        if(distance<best||(distance==best&&f.id<result)){best=distance;result=f.id;}
    }
    return result;
}
inline bool face_present(const std::vector<Face>&faces,double id){for(const auto&f:faces)if(f.id==id)return true;return false;}
inline int nearest_eye(const Rect&left,const Rect&right,double x,double y,double width,double height){
    const Rect eyes[]={left,right};int chosen=-1;double best=std::numeric_limits<double>::infinity();
    for(int i=0;i<2;++i){const auto&r=eyes[i];if(!finite_rect(r)||r.x>=1||r.y>=1||r.x+r.width<=0||r.y+r.height<=0)continue;
        const double dx=(r.x+r.width/2)*width-x,dy=(r.y+r.height/2)*height-y,distance=dx*dx+dy*dy;
        if(distance<best){best=distance;chosen=i;}
    }
    return chosen;
}
struct EyeState {int preferred=0;double face_id=-1;int chosen=0;double missing_since=-1,preferred_since=-1,last_time=-1;};
struct EyeResult {EyeState state;int eye=-1;bool fallback_face=false;};
inline EyeState create_eye(int preferred,double face_id){return {preferred,face_id,preferred,-1,-1,-1};}
inline EyeResult eye_step(EyeState s,int preferred,double face_id,bool left_valid,bool right_valid,double now,double delay){
    // QML 的合法偏好值始终是 0/1；原生边界不允许把外部值作为数组下标。
    if(preferred<0||preferred>1||s.chosen<0||s.chosen>1)return {create_eye(0,-1),-1,false};
    if(s.preferred!=preferred||s.face_id!=face_id||now<s.last_time)s=create_eye(preferred,face_id);
    s.last_time=now;bool valid[]={left_valid,right_valid};if(face_id<0)valid[0]=valid[1]=false;
    if(valid[s.chosen])s.missing_since=-1;else if(s.missing_since<0)s.missing_since=now;
    if(s.chosen!=preferred&&valid[preferred]){if(s.preferred_since<0)s.preferred_since=now;if(now-s.preferred_since>=delay){s.chosen=preferred;s.missing_since=-1;s.preferred_since=-1;}}
    else s.preferred_since=-1;
    if(!valid[s.chosen]&&s.missing_since>=0&&now-s.missing_since>=delay&&valid[1-s.chosen]){s.chosen=1-s.chosen;s.missing_since=-1;s.preferred_since=-1;}
    const int eye=valid[s.chosen]?s.chosen:-1;return {s,eye,face_id>=0&&eye<0&&s.missing_since>=0&&now-s.missing_since>=delay};
}
struct Features {
    bool pixel400,quarter,raw_stack,face_afc,af_speed,playback,display,shutter;
};
// 本轮原生事务固定全功能 255；不给未实现的部分功能清单伪造可安装承诺。
inline constexpr Features all_features{true,true,true,true,true,true,true,true};
}
