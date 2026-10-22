#include "screen_policy.hpp"
#include <QCoreApplication>
#include <QFile>
#include <QJSEngine>
#include <cstdio>
#include <cstdlib>
static int checks, scenario=-1;
static void check(bool value){++checks;if(!value){std::fprintf(stderr,"SCREEN_POLICY_FAIL assertion=%d scenario=%d\n",checks,scenario);std::exit(1);}}
static QString function(const QString&s,const QString&name){auto p=s.indexOf("function "+name+"(");check(p>=0);auto start=p; p=s.indexOf('{',p);int depth=1;while(++p<s.size()&&depth){if(s[p]=='{')++depth;else if(s[p]=='}')--depth;}check(!depth);return s.mid(start,p-start);}
int main(int argc,char**argv){
 QCoreApplication app(argc,argv);check(argc==2);QFile file(QString::fromLocal8Bit(argv[1]));check(file.open(QIODevice::ReadOnly));auto source=QString::fromUtf8(file.readAll());QJSEngine js;
 auto eval=[&](const QString&s){auto v=js.evaluate(s);if(v.isError())std::fprintf(stderr,"JS_ERROR %s\n",qPrintable(v.toString()));check(!v.isError());return v;};
 eval("var console={info:function(){}},root={Window:{window:null}},stateModel=null,evfEntryWanted=false,evfRequested=false;var calls=0;"+function(source,"enterEvfLiveview"));
 for(int bits=0;bits<2048;bits++){
  scenario=bits;
  x2dui::ScreenModeInput in;in.evfOnly=bits&1;in.backAllowed=bits&2;in.evfAllowed=bits&4;in.backActive=bits&8;in.evfActive=bits&16;in.modelPresent=bits&32;in.modelEvf=bits&64;in.shutdown=bits&128;in.upgrade=bits&256;in.maintenance=bits&512;in.exposing=bits&1024;
  auto g=js.globalObject();g.setProperty("evfEntryWanted",in.evfOnly&&in.backAllowed&&in.evfAllowed&&in.evfActive);g.setProperty("evfRequested",false);g.setProperty("calls",0);
  auto model=js.newObject();model.setProperty("screenEVF",in.modelEvf);model.setProperty("isShutdown",in.shutdown);model.setProperty("isMaintenanceUpgrade",in.upgrade);model.setProperty("isMaintenanceManual",in.maintenance);model.setProperty("exposing",in.exposing);model.setProperty("setLiveView",eval("(function(value){if(value)calls++})"));g.setProperty("stateModel",in.modelPresent?model:QJSValue(QJSValue::NullValue));
  x2dui::ScreenModePolicy p;eval("enterEvfLiveview()");check(p.refresh(in)==(g.property("calls").toInt()==1));check(p.intercepting()==(in.evfOnly&&in.backAllowed&&in.evfAllowed&&in.backActive&&!in.evfActive));
  eval("enterEvfLiveview()");check(!p.refresh(in));check(g.property("calls").toInt()<=1);
 }
 const char16_t*labels[]={u"关闭",u"10 秒",u"Off",u"30 秒",u"1 分钟",u"5 分钟"};
 for(int n=0;n<7;n++)for(int current=-1;current<6;current++)for(int zero=0;zero<2;zero++){
  x2dui::ScreenTimeoutChoices c;c.setup(labels,n,u"Off",current<0?u"未知":labels[current],zero);
  int expected[6]{},count=0,index=-1;for(int i=0;i<n;i++)if(i!=0&&i!=2){expected[count]=i;if(i==current)index=count;++count;}
  check(c.valid==(count>0));check(c.count==count);check(c.current==(zero&&count>0?count:index));
  for(int i=-1;i<=count+1;i++){auto selection=c.select(i);check(selection.kind==(!count||i<0||i>count?x2dui::ScreenChoice::None:i==count?x2dui::ScreenChoice::TimeoutOff:x2dui::ScreenChoice::TimeoutStock));if(i>=0&&i<count)check(selection.sourceIndex==expected[i]);}
 }
 x2dui::ScreenTimeoutChoices tooLarge;tooLarge.setup(labels,65,u"Off",u"",false);check(!tooLarge.valid);
 for(int stock=0;stock<5;stock++)for(int selected=-1;selected<5;selected++)for(int mode=0;mode<8;mode++){
  check(x2dui::display_mode_index(stock,selected,mode&1,mode&2,mode&4)==(stock!=3?-1:mode==7?3:selected>=0&&selected<3?selected:-1));
  auto action=x2dui::display_mode_select(stock,selected);check(action.kind==(stock!=3||selected<0||selected>3?x2dui::ScreenChoice::None:selected==3?x2dui::ScreenChoice::DisplaySpecial:x2dui::ScreenChoice::DisplayStock));
 }
 x2dui::ScreenModePolicy p;x2dui::ScreenModeInput in;in.evfOnly=in.backAllowed=in.evfAllowed=in.backActive=true;p.refresh(in);
 auto forward=p.forward(8,8,4);check(forward.write&&forward.value==12);check(!p.forward(12,8,4).write);
 check(!p.pressed(false,false,100));check(p.pressed(true,false,100)&&p.noticeVisible(100));check(p.pressed(true,true,900));check(!p.noticeVisible(1100));check(p.released(true));
 check(p.pressed(true,false,1200)&&p.noticeVisible(1200));in.evfActive=true;in.modelPresent=true;in.modelEvf=true;check(p.refresh(in));check(!p.noticeVisible(1201));auto restore=p.release(16);check(restore.write&&restore.value==16);check(!p.release(16).write);
 std::printf("SCREEN_POLICY_PASS assertions=%d evf_cases=2048 camera_requests=0\n",checks);
}
