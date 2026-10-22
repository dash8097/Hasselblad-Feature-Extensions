#include "quarter_key_policy.hpp"
#include <QCoreApplication>
#include <QFile>
#include <QJSEngine>
#include <cstdio>
#include <cstdlib>
static int assertions;
static void check(bool value){++assertions;if(!value){std::fprintf(stderr,"QUARTER_KEYS_FAIL %d\n",assertions);std::exit(1);}}
static QString handler(QString source,QString name){
 auto begin=source.indexOf(name);check(begin>=0);begin=source.indexOf('{',begin);int end=begin+1,depth=1;
 while(depth&&end<source.size()){if(source[end]=='{')++depth;else if(source[end]=='}')--depth;++end;}
 check(!depth);return source.mid(begin+1,end-begin-2);
}
int main(int argc,char**argv){
 QCoreApplication app(argc,argv);check(argc==2);QFile f(QString::fromLocal8Bit(argv[1]));check(f.open(QIODevice::ReadOnly));QString src=QString::fromUtf8(f.readAll());QJSEngine js;
 QString body=QStringLiteral("var Qt={Key_E:69},KeyFn={HalfPress:1};var MKeys={pressedFn:function(_,e){return e.half},isUnblockableCustomButton:function(e){return e.unblockable}};function press(event){")+handler(src,"Keys.onPressed:")+"};function release(event){"+handler(src,"Keys.onReleased:")+"};";
 check(!js.evaluate(body).isError());
 for(int states=0;states<16;states++)for(int keys=0;keys<8;keys++)for(int release=0;release<2;release++){
  x2dui::QuarterKeyPolicy p;p.active=states&1;p.armed=states&2;p.shutterHeld=states&4;p.halfHeld=states&8;
  auto global=js.globalObject();global.setProperty("activeCapture",p.active);global.setProperty("armedCapture",p.armed);global.setProperty("shutterHeld",p.shutterHeld);global.setProperty("halfHeld",p.halfHeld);
  auto event=js.newObject();event.setProperty("key",keys&1?69:0);event.setProperty("half",bool(keys&2));event.setProperty("unblockable",bool(keys&4));event.setProperty("accepted",false);
  auto value=global.property(release?"release":"press").call({event});check(!value.isError());bool accepted=release?p.released(keys&1,keys&2,keys&4):p.pressed(keys&1,keys&2,keys&4);
  check(accepted==event.property("accepted").toBool());check(p.shutterHeld==global.property("shutterHeld").toBool());check(p.halfHeld==global.property("halfHeld").toBool());check(p.intercepting()==(p.active||p.armed||p.shutterHeld||p.halfHeld));check(p.overrideKeys()==p.active);
 }
 for(int state=0;state<16;state++)for(int conditions=0;conditions<32;conditions++){
  x2dui::QuarterKeyPolicy p;p.active=state&1;p.armed=state&2;p.shutterHeld=state&4;p.halfHeld=state&8;
  check(p.mayRestoreFocus(conditions&1,conditions&2,conditions&4,conditions&8,conditions&16)==(state==0&&conditions==31));
 }
 std::printf("QUARTER_KEYS_PASS assertions=%d keyboard_cases=256 focus_cases=512 camera_requests=0\n",assertions);
}
