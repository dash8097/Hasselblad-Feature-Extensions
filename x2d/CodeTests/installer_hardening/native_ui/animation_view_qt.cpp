#include "animation_view_qt.hpp"
#include <QPainter>
#include <QPainterPath>
#include <QFontMetricsF>
namespace x2dui {
namespace {
QtAnimationPainter&context(void*p){return *static_cast<QtAnimationPainter*>(p);}
bool active(void*p){return p&&context(p).painter&&context(p).painter->isActive();}
QColor color(uint32_t c){return QColor(int(c>>24),int((c>>16)&255),int((c>>8)&255),int(c&255));}
QRectF rect(AnimationRect r){return {r.x,r.y,r.width,r.height};}
QString text(const char16_t*s,size_t n){return QString::fromUtf16(s,qsizetype(n));}
QFont font(QPainter*p,int size){QFont f=p->font();f.setPixelSize(size);return f;}
bool save(void*p){if(!active(p))return false;context(p).painter->save();++context(p).depth;return true;}
void restore(void*p){if(active(p)&&context(p).depth){context(p).painter->restore();--context(p).depth;}}
bool opacity(void*p,double v){if(!active(p))return false;context(p).painter->setOpacity(v);return true;}
bool translate(void*p,double x,double y){if(!active(p))return false;context(p).painter->translate(x,y);return true;}
bool scale(void*p,double x,double y){if(!active(p))return false;context(p).painter->scale(x,y);return true;}
bool rotate(void*p,double a){if(!active(p))return false;context(p).painter->rotate(a);return true;}
bool clip(void*p,AnimationRect r){if(!active(p))return false;context(p).painter->setClipRect(rect(r),Qt::IntersectClip);return true;}
bool fill(void*p,AnimationRect r,uint32_t rgba){if(!active(p))return false;context(p).painter->fillRect(rect(r),color(rgba));return true;}
bool metrics(void*p,const char16_t*s,size_t n,int pixels,AnimationTextMetrics*out){if(!active(p)||!s||n>128||!out||pixels<1||pixels>200)return false;QFontMetricsF m(font(context(p).painter,pixels));*out={m.horizontalAdvance(text(s,n)),m.height(),m.ascent()};return true;}
bool draw_text(void*p,const char16_t*s,size_t n,int pixels,AnimationPoint baseline,uint32_t rgba){if(!active(p)||!s||n>128||pixels<1||pixels>200)return false;auto*q=context(p).painter;q->setFont(font(q,pixels));q->setPen(color(rgba));q->setRenderHint(QPainter::TextAntialiasing,true);q->drawText(QPointF(baseline.x,baseline.y),text(s,n));return true;}
bool path(void*p,const AnimationPathPart*parts,size_t n,uint32_t fillRgba,uint32_t strokeRgba,double width){if(!active(p)||!parts||n!=6)return false;QPainterPath path;path.setFillRule(Qt::WindingFill);
 for(size_t i=0;i<n;++i){auto&v=parts[i];switch(v.verb){case AnimationPathVerb::Move:path.moveTo(v.end.x,v.end.y);break;case AnimationPathVerb::Line:path.lineTo(v.end.x,v.end.y);break;case AnimationPathVerb::Quadratic:path.quadTo(v.control.x,v.control.y,v.end.x,v.end.y);break;case AnimationPathVerb::Close:path.closeSubpath();break;default:return false;}}
 auto*q=context(p).painter;q->setRenderHint(QPainter::Antialiasing,true);q->fillPath(path,QBrush(color(fillRgba)));QPen pen(QBrush(color(strokeRgba)),width,Qt::SolidLine,Qt::FlatCap,Qt::MiterJoin);pen.setMiterLimit(10);q->strokePath(path,pen);return true;
}
}
AnimationPainterApi qt_animation_painter(QtAnimationPainter&c){return {&c,save,restore,opacity,translate,scale,rotate,clip,fill,metrics,draw_text,path};}
static_assert(sizeof(AnimationRect)==32&&sizeof(AnimationPoint)==16&&sizeof(AnimationTextMetrics)==24&&sizeof(AnimationPathPart)==40,"POD ABI footprint");
}
