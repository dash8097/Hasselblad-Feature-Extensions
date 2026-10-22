#include "face_view.hpp"
#include <cfloat>
namespace x2dui {namespace {
constexpr uint32_t white=0xffffffff,gray=0x808080ff,yellow=0xffd400ff,green=0x00ff00ff;
bool finite(double v){return v==v&&v<=DBL_MAX&&v>=-DBL_MAX;}
bool add(FaceFrame&f,FaceDraw d){if(f.count>=560||!finite(d.x)||!finite(d.y)||!finite(d.width)||!finite(d.height)||d.width<0||d.height<0)return false;f.commands[f.count++]=d;return true;}
bool rectangle(FaceFrame&f,double x,double y,double w,double h,uint32_t fill,uint32_t stroke=0,double border=0,double radius=0){return add(f,{radius?FaceDrawKind::RoundedRectangle:FaceDrawKind::Rectangle,x,y,w,h,radius,border,fill,stroke,nullptr,0,0});}
bool corners(FaceFrame&f,double x,double y,double w,double h,double line,uint32_t color){double cw=w*.1,ch=h*.1,bw=std::min(line,cw),bh=std::min(line,ch);for(int i=0;i<4;i++){double cx=x+(i%2?w-cw:0),cy=y+(i>=2?h-ch:0);if(!rectangle(f,cx,cy+(i>=2?ch-bh:0),cw,bh,color)||!rectangle(f,cx+(i%2?cw-bw:0),cy,bw,ch,color))return false;}return true;}
bool eyes(FaceFrame&f,const EyeModel&e){if(!e.visible_eye())return true;auto&i=e.input();auto&s=e.state();for(int n=0;n<2;n++){if(!e.valid_eye(n)||(i.personLocked&&s.active!=n))continue;auto&r=n?i.right:i.left;double w=std::max(24.0,r.width*i.width),h=std::max(24.0,r.height*i.height);uint32_t c=i.focusConfirmed&&i.roi==(n?FaceRoi::EyeRight:FaceRoi::EyeLeft)?green:i.personLocked&&s.active==s.preferred?yellow:white;if(!rectangle(f,(r.x+r.width/2)*i.width-w/2,(r.y+r.height/2)*i.height-h/2,w,h,0,c,2))return false;}return true;}
}
bool face_frame(const FacePointModel&m,const EyeModel&main,const EyeModel&fallback,FaceFrame&out)noexcept{
 out.count=0;out.valid=false;auto&s=m.snapshot();auto&p=m.state();out.width=s.width;out.height=s.height;
 if(!finite(s.width)||!finite(s.height)||s.width<=0||s.height<=0||s.width>16384||s.height>16384||s.count>64)return false;
 if(!s.present||!s.faceMode){out.valid=true;return true;}
 const char16_t*label=p.ordinaryFallback?u"普通点对焦":p.personLocked?(p.lockedRegion.width>0?u"人脸已锁定":u"等待锁定目标"):u"自动选人";size_t len=0;while(label[len])++len;
 if(!add(out,{FaceDrawKind::Text,0,16,s.width,24,0,0,p.personLocked?0xe6a02bff:white,0,label,len,20}))return false;
 if(m.selector_only()||p.pointPending){double w=s.focusRegion.width*s.width,h=s.focusRegion.height*s.height;if(!rectangle(out,p.pointX*s.width-w/2,p.pointY*s.height-h/2,w,h,0,white,2))return false;}
 if(s.canShowAF&&!s.paused)for(size_t i=0;i<s.count;i++){auto&c=s.candidates[i];if(m.actual_face_valid()&&c.id==s.detectedId)continue;auto&r=c.region;if(!corners(out,r.x/10000*s.width,r.y/10000*s.height,r.width/10000*s.width,r.height/10000*s.height,2,gray))return false;}
 auto&r=p.lockedRegion;bool visible=m.actual_face_valid()&&r.width>0&&r.height>0&&s.canShowAF;
 if(visible){double x=r.x*s.width,y=r.y*s.height,w=r.width*s.width,h=r.height*s.height;uint32_t c=s.roi==FaceRoi::Face&&s.foundFocus?green:p.personLocked&&s.detectedId==p.selectedId?yellow:white;
  if(main.visible_eye()||fallback.visible_eye()){if(!corners(out,x,y,w,h,3,c))return false;}else if(!rectangle(out,x,y,w,h,0,c,3))return false;
  if(p.personLocked&&s.detectedId==p.selectedId){double lx=std::max(0.0,std::min(s.width-24,x+w/2-12)),ly=std::max(0.0,y-33);if(!rectangle(out,lx+6,ly+1,12,16,0,c,3,6)||!rectangle(out,lx+2,ly+12,20,15,c,0,0,2)||!rectangle(out,lx+11,ly+17,2,6,0x202020ff))return false;}
 }
 if(!eyes(out,main)||!eyes(out,fallback))return false;out.valid=true;return true;
}
bool face_paint(const FaceFrame&f,const FacePainterApi&a)noexcept{if(!f.valid||f.count>560||!a.begin||!a.draw||!a.end||!a.begin(a.context,f.width,f.height))return false;bool ok=true;for(unsigned i=0;i<f.count;i++)if(!a.draw(a.context,f.commands[i])){ok=false;break;}a.end(a.context);return ok;}
}
