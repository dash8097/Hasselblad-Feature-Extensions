#include "about_display.hpp"
#include "internal_test_about.hpp"
#include <QCoreApplication>
#include <QJSEngine>
#include <cstdio>
#include <cstdlib>
using namespace x2dui;
static int checks;static void check(bool v){++checks;if(!v){std::fprintf(stderr,"ABOUT_FAIL %d\n",checks);std::exit(1);}}
int main(int argc,char**argv){QCoreApplication app(argc,argv);QJSEngine js;
 for(const char*name:{"product_name","model_number","firmware",""})for(bool about:{false,true})for(bool right:{false,true})for(const char16_t*label:{u"X2D 100C",u"100C100C",u"标签",u""})for(const char16_t*formatted:{u"X2D 100C",u"100C 100C",u"No marker",u""}){
  auto result=about_display_text(name,about,right,label,formatted);QString expected=QString::fromUtf16(label);if(about&&right&&(!std::strcmp(name,"product_name")||!std::strcmp(name,"model_number"))&&expected.contains("100C")){auto g=js.globalObject();g.setProperty("value",QString::fromUtf16(formatted));expected=js.evaluate("value.replace(/100C/g,'1600C MS')").toString();}check(QString::fromUtf16(result.data(),qsizetype(result.size()))==expected);
 }
 AboutDiagnostic d;AboutRequest q;check(!d.take(10000,0,q));d.attach(100);check(!d.take(5099,0,q));check(d.take(5100,0,q)&&q.action==AboutAction::ReadRequest);auto id=q.id;check(!d.take(9000,0,q));d.complete(id+1,true,"CHECK_ABOUT\n",9000);check(!d.take(9000,0,q));d.complete(id,true,"CHECK_ABOUT\n",9000);check(d.take(9000,0,q)&&q.action==AboutAction::ConsumeRequest&&q.body=="CONSUMED\n");d.complete(q.id,true,{},9001);check(d.take(9001,0,q)&&q.action==AboutAction::OpenMenu);d.complete(q.id,true,{},9002);check(!d.take(10201,2,q));check(d.take(10202,2,q)&&q.action==AboutAction::WriteResult&&q.body=="ABOUT_DISPLAY_ALIASES=2\n");d.complete(q.id,false,{},10203);check(d.failed()&&d.take(10203,0,q)&&q.action==AboutAction::CloseMenu);d.complete(q.id,true,{},10204);check(d.finished());d.attach(11000);check(!d.take(20000,0,q));
 for(int failed=0;failed<5;failed++){
  AboutDiagnostic s;uint64_t now=5000;s.attach(0);for(int step=0;step<5;step++){if(!s.take(now,17,q))break;auto prior=q.id;s.complete(prior,step!=failed,step==0?ProtocolBytes("CHECK_ABOUT\n"):ProtocolBytes(),now);now+=1200;check(!s.take(0,17,q));}
  check(s.finished());check(s.failed()==(failed!=0));
 }
 AboutDiagnostic gone;gone.attach(0);check(gone.take(5000,0,q));auto old=q.id;gone.detach();gone.complete(old,true,"CHECK_ABOUT\n",6000);check(!gone.take(7000,0,q));gone.attach(8000);check(gone.take(13000,0,q)&&q.id!=old);gone.complete(old,true,"CHECK_ABOUT\n",14000);check(!gone.take(14000,0,q));gone.complete(q.id,true,"OTHER\n",14000);check(gone.finished());
 std::printf("ABOUT_DISPLAY_PASS assertions=%d camera_operations=0\n",checks);
}
