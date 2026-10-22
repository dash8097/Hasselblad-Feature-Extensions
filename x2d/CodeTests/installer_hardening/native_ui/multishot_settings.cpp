#include "multishot_settings.hpp"
#include <cmath>
#include <cstdio>
namespace x2dui {
namespace {
UiText num(int value){char buf[32];std::snprintf(buf,sizeof buf,"%d",value);UiText r;for(auto p=buf;*p;p++)r+=char16_t(*p);return r;}
// 正数 Number.toFixed(2)：直接舍入 IEEE754 的精确二进制值，避免 printf
// 的 ties-to-even 以及先乘100造成的二次舍入；不受进程locale影响。
UiText fixed(double value){
 if(!std::isfinite(value)||value<0||value>=1e12)return u"—";
 uint64_t bits;std::memcpy(&bits,&value,sizeof bits);int exp=int((bits>>52)&2047)-1023-52;
 uint64_t mant=bits&((uint64_t(1)<<52)-1);if((bits>>52)&2047)mant|=uint64_t(1)<<52;else exp=-1074;
 uint64_t scaled=mant*100,rounded=0;
 if(exp>=0)rounded=scaled<<exp;
 else {int shift=-exp;if(shift<64){rounded=scaled>>shift;if((scaled&((uint64_t(1)<<shift)-1))>=(uint64_t(1)<<(shift-1)))++rounded;}}
 auto whole=protocol_number(rounded/100);UiText r;for(char c:whole)r+=char16_t(c);r+=u'.';r+=char16_t('0'+rounded/10%10);r+=char16_t('0'+rounded%10);return r;
}
const char16_t*methods[]={u"最大值",u"最小值",u"平均值"};
bool space(char16_t c){return c==9||c==10||c==11||c==12||c==13||c==32||c==160||c==0x1680||(c>=0x2000&&c<=0x200a)||c==0x2028||c==0x2029||c==0x202f||c==0x205f||c==0x3000||c==0xfeff;}
void add(SettingsChoices&r,int value,const UiText&label){if(r.count>=64||!label.valid()){r.valid=false;return;}r.values[r.count]=value;r.labels[r.count]=label;if(!r.labels[r.count].valid())r.valid=false;++r.count;}
void select(SettingsChoices&r,int value){for(int i=0;i<r.count;i++)if(r.values[i]==value){r.index=i;break;}}
}
int MultishotSettings::source_iso()const{
 if(!data.iso.valid())return 0;size_t first=0,last=data.iso.size();while(first<last&&space(data.iso[first]))++first;while(last>first&&space(data.iso[last-1]))--last;
 if(first==last||last-first>5||data.iso[first]<'1'||data.iso[first]>'9')return 0;int result=0;
 for(size_t i=first;i<last;i++){if(data.iso[i]<'0'||data.iso[i]>'9')return 0;result=result*10+data.iso[i]-'0';}return result<=65535?result:0;
}
int MultishotSettings::planned_frames()const{
 if(data.controller&&(data.busy||data.applied)&&data.controllerIso&&data.controllerTargetIso==data.targetIso&&data.controllerPlannedFrames>0)return data.controllerPlannedFrames;
 int source=source_iso();return data.targetIso>0&&source>=data.targetIso?(source+data.targetIso-1)/data.targetIso:0;
}
int MultishotSettings::row_count()const{return !data.stack||editingTime?3:8;}
bool MultishotSettings::row_editable(int row)const{
 if(row<0||row>=row_count())return false;
 return data.stack?editingTime||row==0||row==1||row==4:row<=2&&!(row>0&&data.controller&&data.resolution==25);
}
UiText MultishotSettings::duration(int s){if(s<0||s>86399)return u"—";UiText r;int p[]={s/3600,s/60%60,s%60};for(int i=0;i<3;i++){if(i)r+=u':';if(p[i]<10)r+=u'0';r+=num(p[i]);}return r;}
UiText MultishotSettings::title()const{return data.stack?(editingTime?u"总时间":u"RAW 堆栈"):u"像素超频";}
UiText MultishotSettings::label(int row)const{
 if(row<0||row>=row_count())return {};
 const char16_t*pixel[]={u"像素数量",u"初始延迟",u"拍摄结束后"};const char16_t*parts[]={u"小时",u"分钟",u"秒"};
 const char16_t*stack[]={u"堆栈方式",u"总时间",u"帧数",u"间隙时间",u"初始延迟",u"光圈",u"快门",u"ISO"};
 if(!data.stack)return pixel[row];if(editingTime)return parts[row];return stack[row];
}
UiText MultishotSettings::value(int row)const{
 if(row<0||row>=row_count())return u"—";
 if(data.stack){
  if(editingTime)return num(row==0?data.totalSeconds/3600:row==1?data.totalSeconds/60%60:data.totalSeconds%60);
  if(row==0)return data.method>=0&&data.method<3?methods[data.method]:u"—";
  if(row==1)return duration(data.totalSeconds);
  if(row==2){return data.frames>=0?num(data.frames)+u" 帧":UiText(u"—");}
  if(row==3)return data.frames>=2&&data.gapSeconds>=0?fixed(data.gapSeconds)+u" 秒":UiText(u"—");
  if(row==4)return data.stackDelay==0?UiText(u"关闭"):num(data.stackDelay)+u" 秒";
  if(row==5)return data.aperture.empty()?UiText(u"—"):data.aperture;
  if(row==6)return data.shutter.empty()?UiText(u"—"):data.shutter;
  if(row==7)return data.iso.empty()?UiText(u"—"):data.iso;
  return u"—";
 }
 if(!data.controller)return u"—";
 if(row==0)return data.resolution==25?u"25MP":data.resolution==16?u"十六亿":u"四亿";
 if(row==1)return data.resolution==25?UiText(u"关闭"):num(data.pixelDelay)+u" 秒";
 return data.resolution==25||data.keep?u"保持":u"退出";
}
bool MultishotSettings::open_row(int row){
 if(!editable()||!row_editable(row))return false;
 if(data.stack&&!editingTime&&row==1){editingTime=true;currentRow=0;return true;}
 currentRow=row;popup=true;return true;
}
SettingsExit MultishotSettings::leave(bool external){if(editingTime&&!external){editingTime=false;currentRow=1;return SettingsExit::None;}return external?SettingsExit::External:SettingsExit::Back;}
void MultishotSettings::move(int direction){
 if(!direction)return;
 if(!data.stack||editingTime){currentRow+=direction>0?1:-1;if(currentRow<0)currentRow=0;if(currentRow>2)currentRow=2;return;}
 currentRow=direction>0?(currentRow==0?1:4):(currentRow>=4?1:0);
}
SettingsChoices MultishotSettings::choices()const{
 SettingsChoices r;if(!editable()||!row_editable(currentRow)){r.valid=false;return r;}
 int selected=0;
 if(data.stack){
  if(editingTime){r.option=currentRow==0?SettingsOption::Hours:currentRow==1?SettingsOption::Minutes:SettingsOption::Seconds;for(int i=0;i<=(currentRow==0?23:59);i++)add(r,i,num(i));selected=currentRow==0?data.totalSeconds/3600:currentRow==1?data.totalSeconds/60%60:data.totalSeconds%60;}
  else if(currentRow==0){r.option=SettingsOption::Method;for(int i=0;i<3;i++)add(r,i,methods[i]);selected=data.method;}
  else if(currentRow==4){r.option=SettingsOption::StackDelay;for(int i=0;i<=60;i++)add(r,i,i?num(i)+u" 秒":UiText(u"关闭"));selected=data.stackDelay;}
  else r.valid=false;
 }else if(currentRow==0){r.option=SettingsOption::Resolution;if(data.quarter)add(r,25,u"25MP");if(data.pixel400){add(r,6,u"四亿");add(r,16,u"十六亿");}selected=data.resolution;}
 else if(currentRow==1){r.option=SettingsOption::PixelDelay;for(int i=2;i<=60;i++)add(r,i,num(i)+u" 秒");selected=data.pixelDelay;}
 else {r.option=SettingsOption::Keep;add(r,0,u"退出");add(r,1,u"保持");selected=data.keep;}
 select(r,selected);if(!r.count)r.valid=false;return r;
}
SettingsChange MultishotSettings::choose(SettingsOption option,double value){
 SettingsChange out;if(!editable()||!std::isfinite(value)||value!=std::floor(value)||value<0||value>65535)return out;int n=int(value);
 if(!data.stack){if(option==SettingsOption::Resolution&&((data.quarter&&n==25)||(data.pixel400&&(n==6||n==16))))data.resolution=n;
  else if(option==SettingsOption::PixelDelay&&data.resolution!=25&&n>=2&&n<=60)data.pixelDelay=n;
  else if(option==SettingsOption::Keep&&data.resolution!=25&&n<=1)data.keep=n;
  else return out;return {option,n,0};}
 if(option==SettingsOption::StackDelay&&n<=60)data.stackDelay=n;
 else if(option==SettingsOption::Method&&n<=2){data.method=n;currentRow=0;}
 else if(editingTime&&(option==SettingsOption::Hours||option==SettingsOption::Minutes||option==SettingsOption::Seconds)){
  int index=option==SettingsOption::Hours?0:option==SettingsOption::Minutes?1:2;if(n>(index==0?23:59))return out;
  int parts[]={data.totalSeconds/3600,data.totalSeconds/60%60,data.totalSeconds%60};parts[index]=n;int seconds=parts[0]*3600+parts[1]*60+parts[2];if(seconds<1)return out;data.totalSeconds=seconds;option=SettingsOption::TotalSeconds;n=seconds;
 }else return out;out.option=option;out.value=n;return out;
}
}
