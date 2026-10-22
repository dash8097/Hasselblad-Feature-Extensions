#include "face_view_qt.hpp"
#include <QPainter>
#include <QPainterPath>
#include <QFontMetricsF>
namespace x2dui {namespace {
QColor c(uint32_t v){return QColor(v>>24,(v>>16)&255,(v>>8)&255,v&255);}
bool begin(void*p,double w,double h){auto&t=*static_cast<FaceQtPainter*>(p);if(!t.painter||!t.painter->isActive())return false;t.painter->save();++t.depth;t.painter->setClipRect(QRectF(0,0,w,h));return true;}
void end(void*p){auto&t=*static_cast<FaceQtPainter*>(p);if(t.depth){t.painter->restore();--t.depth;}}
bool draw(void*p,const FaceDraw&d){auto*q=static_cast<FaceQtPainter*>(p)->painter;if(!q||!q->isActive())return false;
 if(d.kind==FaceDrawKind::Text){if(!d.text||d.length>64||d.pixels<1||d.pixels>200)return false;QFont font=q->font();font.setPixelSize(d.pixels);q->setFont(font);QFontMetricsF m(font);auto s=QString::fromUtf16(d.text,qsizetype(d.length));q->setPen(c(d.fill));q->setRenderHint(QPainter::TextAntialiasing,true);q->drawText(QPointF(d.x+(d.width-m.horizontalAdvance(s))/2,d.y+m.ascent()),s);return true;}
 QRectF r(d.x,d.y,d.width,d.height);q->setRenderHint(QPainter::Antialiasing,d.radius>0);q->setPen(Qt::NoPen);
 if(!d.radius){if(d.fill&255)q->fillRect(r,c(d.fill));double w=std::min(d.border,d.width/2),h=std::min(d.border,d.height/2);if(d.border>0&&(d.stroke&255)){q->fillRect(QRectF(d.x,d.y,d.width,h),c(d.stroke));q->fillRect(QRectF(d.x,d.y+d.height-h,d.width,h),c(d.stroke));q->fillRect(QRectF(d.x,d.y+h,w,std::max(0.,d.height-h*2)),c(d.stroke));q->fillRect(QRectF(d.x+d.width-w,d.y+h,w,std::max(0.,d.height-h*2)),c(d.stroke));}return true;}
 q->setBrush(c(d.fill));if(d.border>0){q->setPen(QPen(c(d.stroke),d.border));r.adjust(d.border/2,d.border/2,-d.border/2,-d.border/2);}q->drawRoundedRect(r,std::max(0.,d.radius-d.border/2),std::max(0.,d.radius-d.border/2));return true;
}
}
FacePainterApi face_qt_painter(FaceQtPainter&p){return {&p,begin,draw,end};}
}
