#include "processing_presentation.hpp"
#include <QCoreApplication>
#include <QFile>
#include <QJSEngine>
#include <QJSValue>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstdio>
#include <cstdlib>
#include <cmath>
static int checks;
static void check(bool yes,const char*name){++checks;if(!yes){std::fprintf(stderr,"PROCESSING_FAIL %s case=%d\n",name,checks);std::exit(1);}}
static QString body(const QString&src,const QString&marker){
 auto p=src.indexOf(marker);check(p>=0,"oracle-source-marker");p=src.indexOf('{',p);auto start=p+1;int depth=1;
 while(++p<src.size()&&depth){if(src[p]=='{')++depth;else if(src[p]=='}')--depth;}
 return src.mid(start,p-start-1);
}
int main(int argc,char**argv){
 QCoreApplication app(argc,argv);check(argc==2,"oracle-path");QFile file(QString::fromLocal8Bit(argv[1]));check(file.open(QIODevice::ReadOnly),"oracle-read");QString src=QString::fromUtf8(file.readAll());
 QJSEngine js;auto eval=[&](const QString&s){auto v=js.evaluate(s);check(!v.isError(),"JS-oracle-evaluation");return v;};
 eval("function status(){"+body(src,"function stageText()")+"};function quantity(){"+body(src,"text2.text: {")+"};function detail(){"+body(src,"text3.text: {")+"}");
 const char*stages[]={"idle","prepare","delay","capture","merge","render","jpeg","jpeg_join","package","convert","raw","publish","restore","cleanup","complete","failed","unknown"};
 for(int stage=0;stage<17;stage++)for(int mode=0;mode<4;mode++)for(int flags=0;flags<16;flags++)for(int sample=0;sample<3;sample++){
  x2dui::ProcessingState s;s.present=true;s.stage=static_cast<x2dui::ProcessingStage>(stage);s.busy=flags&1;s.ready=flags&2;s.preparing=flags&4;s.joinComplete=flags&8;s.stack=mode>=2;s.iso=mode==3;s.resolution=mode==1?16:6;
  s.captureFrames=6;s.stackRemaining=sample==0?0:12;s.stackFrames=sample==0?1:5;s.plannedFrames=sample==0?0:24;s.renderTiles=sample==0?0:sample==1?8:26;s.jpegTiles=s.renderTiles;
  s.gapMs=1230;s.outputIso=sample==0?0:128.5;s.joinRows=sample==0?0:sample==1?18:25;s.joinTotal=sample==0?0:25;s.workDone=3;s.workTotal=sample==0?0:8;s.applicationError=sample==1?u"示例错误":u"";s.lastError=u"后台失败";s.shutter=u"1/100";
  QJsonObject c{{"stage",stages[stage]},{"busy",s.busy},{"ready",s.ready},{"preparingMode",s.preparing},{"stackMode",s.stack},{"stackIsoMode",s.iso},{"pixelResolution",s.resolution},{"captureFrames",s.captureFrames},{"stackRemaining",s.stackRemaining},{"stackFrameCount",s.stackFrames},{"stackPlannedFrames",s.plannedFrames},{"renderTiles",s.renderTiles},{"jpegTiles",s.jpegTiles},{"stackGapMs",s.gapMs},{"stackOutputIso",s.outputIso},{"jpegJoinRows",s.joinRows},{"jpegJoinTotal",s.joinTotal},{"jpegJoinComplete",s.joinComplete},{"workDone",s.workDone},{"workTotal",s.workTotal},{"lastError",QString::fromUtf16(s.lastError)},{"stackOutputShutterText",QString::fromUtf16(s.shutter)}};
  eval("var controller="+QString::fromUtf8(QJsonDocument(c).toJson(QJsonDocument::Compact))+";var applicationError="+(sample==1?QStringLiteral("'示例错误'"):QStringLiteral("''"))+";var root={applicationError:applicationError};");
  auto p=x2dui::processing_presentation(s);
  check(QString::fromUtf16(p.status)==eval("status()").toString(),"status-differential");
  check(QString::fromUtf16(p.quantity)==eval("quantity()").toString(),"quantity-differential");
  check(QString::fromUtf16(p.detail)==eval("detail()").toString(),"detail-differential");
  check(p.showButtons==(!s.busy||(s.stack&&s.busy&&stage==3)||stage==2),"button-contract-with-countdown-cancel");
  check(std::isfinite(p.progress)&&p.progress>=0&&p.progress<=1,"bounded-progress");
 }
 x2dui::ProcessingState absent;auto p=x2dui::processing_presentation(absent);check(QString::fromUtf16(p.status)==QStringLiteral("后台未就绪")&&!p.showButtons&&!p.progress,"missing-controller");
 x2dui::ProcessingState cancel;cancel.present=cancel.busy=true;cancel.stage=x2dui::ProcessingStage::Delay;p=x2dui::processing_presentation(cancel);check(p.cancelAllowed&&p.showButtons&&QString::fromUtf16(p.button)==QStringLiteral("取消"),"countdown-cancel-visible");cancel.busy=false;cancel.stage=x2dui::ProcessingStage::Failed;cancel.lastError=u"拍摄已取消";p=x2dui::processing_presentation(cancel);check(QString::fromUtf16(p.status)==QStringLiteral("已取消")&&p.showButtons&&!p.cancelAllowed,"cancelled-return-state");
 x2dui::ProcessingState invalid;invalid.present=true;invalid.busy=true;invalid.workDone=INFINITY;invalid.workTotal=1;p=x2dui::processing_presentation(invalid);check(p.progress==0,"nonfinite-progress");
 std::printf("PROCESSING_PRESENTATION_PASS assertions=%d scenarios=3264 oracle=original-p23-QML camera_requests=0\n",checks);
}
