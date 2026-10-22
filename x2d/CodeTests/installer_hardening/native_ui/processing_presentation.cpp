#include "processing_presentation.hpp"
#include <cstdio>
#include <cmath>
namespace x2dui {
namespace {
template<std::size_t N> void append(char16_t(&out)[N],const char16_t*text){
 std::size_t n=0;while(n+1<N&&out[n])++n;if(!text)return;
 while(n+1<N&&*text)out[n++]=*text++;out[n]=0;
 // Do not expose an incomplete surrogate pair after truncation.
 if(n&&out[n-1]>=0xd800&&out[n-1]<=0xdbff)out[n-1]=0;
}
template<std::size_t N> void number(char16_t(&out)[N],double value,int digits=0){
 char ascii[80];if(!std::isfinite(value)){append(out,u"—");return;}
 std::snprintf(ascii,sizeof ascii,digits==2?"%.2f":"%.0f",value);
 char16_t text[80]{};for(unsigned i=0;i<79&&ascii[i];++i)text[i]=static_cast<unsigned char>(ascii[i]);append(out,text);
}
double bound(double x,double high){return !std::isfinite(x)||x<0?0:x>high?high:x;}
int min(int a,int b){return a<b?a:b;}
bool nonempty(const char16_t*s){return s&&*s;}
bool same(const char16_t*a,const char16_t*b){if(!a||!b)return false;while(*a&&*a==*b){++a;++b;}return *a==*b;}
}
ProcessingPresentation processing_presentation(const ProcessingState&s){
 ProcessingPresentation p;using S=ProcessingStage;
 append(p.title,s.present&&s.stack?(s.iso?u"ISO 堆栈":u"RAW 堆栈"):u"像素超频");
 p.counting=s.present&&s.stage==S::Delay;
 p.finishAllowed=s.present&&s.stack&&s.busy&&s.stage==S::Capture;
 p.cancelAllowed=s.present&&s.busy&&s.stage==S::Delay;
 p.showButtons=s.present&&(!s.busy||p.finishAllowed||p.cancelAllowed);
 append(p.button,p.cancelAllowed?u"取消":p.finishAllowed||(s.present&&s.stage==S::Complete)?u"完成":u"返回");
 append(p.countdown,s.countdown);
 if(nonempty(s.applicationError)&&s.present&&!s.busy)append(p.status,u"模式应用失败");
 else if(s.present&&s.preparing)append(p.status,u"正在准备");
 else if(s.present&&s.stage==S::Failed)append(p.status,same(s.lastError,u"拍摄已取消")?u"已取消":u"处理失败");
 else if(s.present&&s.stack&&s.stage==S::Capture){if(s.stackRemaining>0){number(p.status,s.stackRemaining);append(p.status,u" 秒");}else append(p.status,u"收尾中");}
 else if(!s.present||(!s.ready&&!s.busy))append(p.status,u"后台未就绪");
 else {
  const char16_t*label=u"处理中";
  switch(s.stage){
   case S::Idle:label=u"已就绪";break;case S::Prepare:label=u"正在准备";break;case S::Delay:label=u"等待拍摄";break;
   case S::Capture:append(p.status,u"拍摄 ");number(p.status,s.captureFrames);label=u" 张";break;
   case S::Merge:label=u"合成中";break;case S::Render:label=!s.stack&&s.resolution==6&&s.renderTiles<9?u"显影":u"生成 JPEG";break;
   case S::Jpeg:case S::JpegJoin:label=u"生成 JPEG";break;case S::Package:label=u"封装成片";break;case S::Convert:label=u"转换 HEIF";break;
   case S::Raw:label=u"保存 3FR";break;case S::Publish:label=u"保存成片";break;case S::Restore:label=u"恢复相机";break;
   case S::Cleanup:label=u"清理中间文件";break;case S::Complete:label=u"成片已保存";break;case S::Failed:label=u"处理失败";break;default:break;
  }append(p.status,label);
 }
 if(s.present&&!s.preparing&&s.stage!=S::Failed){
  if(s.stack&&s.stage==S::Capture){number(p.quantity,s.stackFrames);append(p.quantity,u" 帧");}
  else if(!s.stack&&(s.resolution==6||s.resolution==16)){
   int total=s.resolution==16?25:9;
   if(s.stage==S::JpegJoin){number(p.quantity,s.joinComplete?100:s.joinTotal>0?bound(std::floor(s.joinRows*100/s.joinTotal),99):0);append(p.quantity,u"%");}
   else if(s.stage==S::Jpeg||(s.stage==S::Render&&(total==25||s.renderTiles<9))){
    number(p.quantity,s.stage==S::Jpeg?min(s.jpegTiles,total-1)+1:total==25?min(s.renderTiles,total-1)+1:s.renderTiles);append(p.quantity,u"/");number(p.quantity,total);
   }
  }
 }
 if(s.present){
  if(!s.busy)append(p.detail,nonempty(s.applicationError)?s.applicationError:s.lastError);
  else if(s.iso&&s.stage==S::Capture){
   if(s.plannedFrames>0){append(p.detail,u"计划 ");number(p.detail,s.plannedFrames);append(p.detail,u" 帧");}else append(p.detail,u"读取拍摄参数");
   append(p.detail,u"\n成片 ISO ");if(s.outputIso>0)number(p.detail,s.outputIso,2);else append(p.detail,u"—");append(p.detail,u" · 快门 ");append(p.detail,s.shutter);
  }else if(s.stack&&s.stage==S::Capture){append(p.detail,u"帧间隔 ");if(s.stackFrames>=2){number(p.detail,s.gapMs/1000,2);append(p.detail,u" 秒");}else append(p.detail,u"—");}
  else if(s.stage==S::Prepare||s.stage==S::Delay||s.stage==S::Capture)append(p.detail,u"请保持相机稳定");
  else if(s.stage==S::JpegJoin)append(p.detail,s.joinRows==s.joinTotal&&s.joinTotal>0&&!s.joinComplete?u"成片写入中":u"完成 9/9 块编码");
  else if(s.workTotal>0){append(p.detail,u"已完成 ");number(p.detail,s.workDone);append(p.detail,u"/");number(p.detail,s.workTotal);append(p.detail,u" 步");}
  else append(p.detail,u"采集已结束");
 }
 p.infinite=s.present&&s.stage==S::JpegJoin&&s.joinTotal==0;
 p.progress=!s.present?0:s.stage==S::Complete?1:s.stage==S::JpegJoin&&s.joinTotal>0?(s.joinComplete?1:bound(s.joinRows/s.joinTotal,.99)):s.workTotal>0?bound(s.workDone/s.workTotal,1):0;
 return p;
}
}
