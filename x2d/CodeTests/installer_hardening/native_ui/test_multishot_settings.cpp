#include "multishot_settings.hpp"
#include "protocol_status.hpp"
#include <QCoreApplication>
#include <QFile>
#include <QJSEngine>
#include <cmath>
#include <cstdio>
#include <cstdlib>
using namespace x2dui;
static unsigned assertions;static int scenario=-1;
static void check(bool v){++assertions;if(!v){std::fprintf(stderr,"SETTINGS_FAIL assertion=%u scenario=%d\n",assertions,scenario);std::exit(1);}}
static QString str(const UiText&s){check(s.valid());return QString::fromUtf16(s.data(),qsizetype(s.size()));}
static QString func(const QString&s,const QString&name){auto start=s.indexOf("function "+name+"(");check(start>=0);auto p=s.indexOf('{',start);int depth=1;while(++p<s.size()&&depth){if(s[p]=='{')++depth;else if(s[p]=='}')--depth;}check(!depth);return s.mid(start,p-start)+"\n";}
int main(int argc,char**argv){
 QCoreApplication app(argc,argv);check(argc==2);QFile f(QString::fromLocal8Bit(argv[1]));check(f.open(QIODevice::ReadOnly));QString original=QString::fromUtf8(f.readAll());QJSEngine js;
 auto eval=[&](const QString&s){auto r=js.evaluate(s);if(r.isError())std::fprintf(stderr,"%s\n",qPrintable(r.toString()));check(!r.isError());return r;};
 QString pre="var stackMethodLabels=['最大值','最小值','平均值','ISO 堆栈'];var controller,exposureModel,stackSettings,editingTotalTime,stackMethod,stackTargetIso,stackTotalSeconds,stackFrameCount,stackGapSeconds,stackInitialDelay,averageNoiseReduction,isoStack,sourceIso,plannedFrames,editable,currentRow,events;";
 for(auto name:{"stackMethodChosen","averageNoiseReductionChosen","stackTotalSecondsChosen","stackInitialDelayChosen","stackTargetIsoChosen"})pre+="function "+QString(name)+"(v){events.push(['"+name+"',v]);}\n";
 eval(pre+func(original,"durationText")+func(original,"rowEditable")+func(original,"valueText")+func(original,"choose"));
 auto setup=[&](const MultishotSettings&m){auto g=js.globalObject();auto&d=m.data;
  g.setProperty("stackSettings",d.stack);g.setProperty("editingTotalTime",m.editingTime);g.setProperty("stackMethod",d.method);g.setProperty("stackTargetIso",d.targetIso);g.setProperty("stackTotalSeconds",d.totalSeconds);g.setProperty("stackFrameCount",d.frames);g.setProperty("stackGapSeconds",d.gapSeconds);g.setProperty("stackInitialDelay",d.stackDelay);g.setProperty("averageNoiseReduction",d.noise);g.setProperty("isoStack",d.stack&&d.method==3);g.setProperty("sourceIso",m.source_iso());g.setProperty("plannedFrames",m.planned_frames());g.setProperty("editable",m.editable());g.setProperty("currentRow",m.currentRow);g.setProperty("events",js.newArray());
  auto c=js.newObject();c.setProperty("pixelResolution",d.resolution);c.setProperty("initialDelay",d.pixelDelay);c.setProperty("keepMode",d.keep);c.setProperty("stackOutputIso",d.outputIso);c.setProperty("stackOutputSeconds",d.outputSeconds);c.setProperty("stackOutputShutterText",str(d.outputShutter));g.setProperty("controller",d.controller?c:QJSValue(QJSValue::NullValue));
  auto e=js.newObject();e.setProperty("aperture",str(d.aperture));e.setProperty("shutterSpeed",str(d.shutter));e.setProperty("isoAndAutoAsString",str(d.iso));g.setProperty("exposureModel",e);
 };
 for(int bits=0;bits<1024;bits++){
  scenario=bits;MultishotSettings m;m.data.stack=bits&1;m.editingTime=m.data.stack&&(bits&2);m.data.method=((bits>>2)&3)%3;m.data.controller=bits&16;m.data.busy=bits&32;m.data.resolution=(bits&64)?25:(bits&128)?16:6;m.data.keep=bits&256;m.data.noise=bits&512;
  m.data.totalSeconds=bits*73%86400;m.data.frames=bits%5-1;m.data.gapSeconds=bits*.125;m.data.stackDelay=bits%61;m.data.pixelDelay=2+bits%59;m.data.targetIso=100;m.data.iso=u" 640 ";m.data.aperture=u"f/4";m.data.shutter=u"1/125";m.data.outputIso=125.125;m.data.outputSeconds=1.25;m.data.outputShutter=u"1.25 s";
  setup(m);for(int row=0;row<m.row_count();row++){
   auto value=eval("valueText("+QString::number(row)+")").toString();if(str(m.value(row))!=value)std::fprintf(stderr,"VALUE row=%d C=%s JS=%s\n",row,qPrintable(str(m.value(row))),qPrintable(value));check(str(m.value(row))==value);
   check(m.row_editable(row)==eval("rowEditable("+QString::number(row)+")").toBool());check(!m.label(row).empty());
  }
 }
 const char*keys[]={"stackInitialDelay","stackTargetIso","stackMethod","averageNoiseReduction","durationHours","durationMinutes","durationSeconds","unknown"};
 SettingsOption opts[]={SettingsOption::StackDelay,SettingsOption::TargetIso,SettingsOption::Method,SettingsOption::Noise,SettingsOption::Hours,SettingsOption::Minutes,SettingsOption::Seconds,SettingsOption::None};
 double values[]={-1,0,1,2,3,23,24,59,60,61,100,65535,65536,1.5,INFINITY,NAN};
 for(int bits=0;bits<32;bits++)for(int k=0;k<8;k++)for(double number:values){
  // 原 QML 的降噪选项已按用户要求退役；单独断言拒绝，不再要求旧行为等价。
  if(opts[k]==SettingsOption::Noise||opts[k]==SettingsOption::TargetIso||(opts[k]==SettingsOption::Method&&number==3)){MultishotSettings retired;retired.data.stack=retired.data.controller=true;retired.data.method=bits&3;check(retired.choose(opts[k],number).option==SettingsOption::None);continue;}
  scenario=2000+bits*8+k;MultishotSettings m;m.data.stack=true;m.data.controller=true;m.data.method=bits%3;m.editingTime=bits&4;m.data.busy=bits&8;m.data.totalSeconds=bits&16?1:3661;m.data.targetIso=800;m.data.iso=u"400";setup(m);auto g=js.globalObject();g.setProperty("key",keys[k]);g.setProperty("inputValue",number);bool accepted=eval("choose(key,inputValue)").toBool();auto result=m.choose(opts[k],number);check(accepted==(result.option!=SettingsOption::None));
  check(m.data.method==g.property("stackMethod").toInt());check(m.data.targetIso==g.property("stackTargetIso").toInt());check(m.data.totalSeconds==g.property("stackTotalSeconds").toInt());check(m.data.stackDelay==g.property("stackInitialDelay").toInt());check(m.data.noise==g.property("averageNoiseReduction").toBool());check(m.currentRow==g.property("currentRow").toInt());
  if(result.alsoTargetIso)check(g.property("events").property("length").toInt()==2);
 }
 for(int n=0;n<86400;n+=137){MultishotSettings m;check(str(m.duration(n))==eval("durationText("+QString::number(n)+")").toString());}
 struct Iso {const char16_t*text;int expected;};for(auto example:{Iso{u"100",100},Iso{u"\u3000 640 \u00a0",640},Iso{u"Auto 100",0},Iso{u"00100",0},Iso{u"65535",65535},Iso{u"65536",0},Iso{u"0",0},Iso{u"1e2",0},Iso{u"",0}}){MultishotSettings m;m.data.iso=example.text;check(m.source_iso()==example.expected);}
 for(int bits=0;bits<32;bits++)for(int source:{0,1,2,100,400,65535})for(int target:{1,3,100,500,65535}){
  MultishotSettings m;m.data.controller=true;m.data.stack=bits&1;m.editingTime=m.data.stack&&(bits&2);m.data.method=((bits>>2)&3)%3;m.data.resolution=bits&16?25:6;m.data.targetIso=target;m.data.iso=protocol_utf16(protocol_number(source));
  for(int row=0;row<m.row_count();row++){m.currentRow=row;auto choices=m.choices();if(!m.row_editable(row)){check(!choices.valid);continue;}if(m.data.stack&&!m.editingTime&&row==1&&m.data.method!=3){check(m.open_row(row)&&m.editingTime&&!m.popup);m.editingTime=false;continue;}
   check(choices.valid&&choices.count>0&&choices.count<=64);for(int i=0;i<choices.count;i++){check(!choices.labels[i].empty());if(choices.option==SettingsOption::TargetIso){check(!source||choices.values[i]<=source);if(i)check(choices.values[i]>choices.values[i-1]);}}
  }
 }
 for(int method=0;method<3;method++){MultishotSettings m;m.data.controller=m.data.stack=true;m.data.method=method;check(m.row_count()==8);check(!m.row_editable(8));m.move(1);check(m.currentRow==1);m.move(1);check(m.currentRow==4);m.move(1);check(m.currentRow==4);m.move(-1);check(m.currentRow==1);}
 MultishotSettings m;m.data.controller=m.data.stack=true;check(m.open_row(1)&&m.editingTime);check(m.leave(false)==SettingsExit::None&&!m.editingTime&&m.currentRow==1);check(m.leave(true)==SettingsExit::External);m.data.busy=true;check(!m.open_row(0));
 std::printf("MULTISHOT_SETTINGS_PASS assertions=%u value_scenarios=1024 choice_scenarios=4096 camera_requests=0\n",assertions);
}
