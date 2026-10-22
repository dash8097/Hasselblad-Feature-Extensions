// 仅内部测试加载旧策略 JS 作为差分 oracle；产品头文件不依赖 QJSEngine。
#include "selection_policies.hpp"
#include <QtCore/QCoreApplication>
#include <QtCore/QFile>
#include <QtQml/QJSEngine>
#include <cstdio>
#include <cstdlib>
#include <random>
using namespace x2d::selection;
static unsigned checks;
static void check(bool ok,const char*why){++checks;if(!ok){fprintf(stderr,"SELECTION_FAILED %s %u\n",why,checks);std::exit(1);}}
static QJSValue rect(QJSEngine&e,const Rect&r){if(!r.present)return QJSValue(QJSValue::NullValue);auto v=e.newObject();v.setProperty("x",r.x);v.setProperty("y",r.y);v.setProperty("width",r.width);v.setProperty("height",r.height);return v;}
static void load(QJSEngine&e,const char*p){QFile f(QString::fromLocal8Bit(p));check(f.open(QIODevice::ReadOnly),"source open");auto text=QString::fromUtf8(f.readAll()).replace(".pragma library","");check(!e.evaluate(text).isError(),"oracle load");}
int main(int argc,char**argv){QCoreApplication app(argc,argv);check(argc==3,"args");QJSEngine faces_js,eyes_js;load(faces_js,argv[1]);load(eyes_js,argv[2]);std::mt19937 rng(420255);std::uniform_real_distribution<double> value(-1.5,1.5);
    for(unsigned n=0;n<500;++n){std::vector<Face> faces;auto array=faces_js.newArray(12);for(unsigned i=0;i<12;++i){Face f{double(rng()%65540)-2,{value(rng)*10000,value(rng)*10000,value(rng)*10000,value(rng)*10000,true}};if(n%17==0)f.id=double(i%3);faces.push_back(f);auto object=rect(faces_js,f.rect);object.setProperty("id",f.id);array.setProperty(i,object);}double x=value(rng)*10000,y=value(rng)*10000;auto js=faces_js.globalObject().property("nearest").call({array,x,y});check(!js.isError()&&js.toNumber()==nearest_face(faces,x,y),"nearest face differential");double id=faces[n%12].id;check(faces_js.globalObject().property("present").call({array,id}).toBool()==face_present(faces,id),"present differential");}
    check(nearest_face({{9,{0,0,10,10,true}},{2,{0,0,10,10,true}}},5,5)==2,"tie stable ID");
    check(nearest_face({{4,{0,0,10,10,true}}},std::numeric_limits<double>::quiet_NaN(),5)==-1,"invalid point");
    for(unsigned n=0;n<500;++n){Rect a{value(rng),value(rng),value(rng),value(rng),n%9!=0},b{value(rng),value(rng),value(rng),value(rng),n%11!=0};double x=value(rng)*1024,y=value(rng)*768;auto js=eyes_js.globalObject().property("nearestEye").call({rect(eyes_js,a),rect(eyes_js,b),x,y,1024,768});check(!js.isError()&&js.toInt()==nearest_eye(a,b,x,y,1024,768),"eye coordinate differential");}
    EyeState state=create_eye(0,7);auto js_state=eyes_js.globalObject().property("create").call({0,7});double now=0;int preferred=0;double face=7;
    for(unsigned n=0;n<1000;++n){now+=double(rng()%120);if(n%47==0)now-=500;if(n%53==0)preferred=1-preferred;if(n%37==0)face=face<0?7:-1;bool l=rng()%3!=0,r=rng()%3!=0;double delay=n%7==0?0:300;auto native=eye_step(state,preferred,face,l,r,now,delay);auto js=eyes_js.globalObject().property("step").call({js_state,preferred,face,l,r,now,delay});check(!js.isError(),"eye step oracle");js_state=js.property("state");state=native.state;check(js.property("eye").toInt()==native.eye&&js.property("fallbackFace").toBool()==native.fallback_face,"eye choice differential");check(js_state.property("chosen").toInt()==state.chosen&&js_state.property("missingSince").toNumber()==state.missing_since&&js_state.property("preferredSince").toNumber()==state.preferred_since&&js_state.property("lastTime").toNumber()==state.last_time,"eye hysteresis state differential");}
    check(eye_step(state,99,7,true,true,0,300).eye==-1,"invalid preference rejected");
    printf("SELECTION_POLICIES_PASS assertions=%u differential_samples=2000\n",checks);return 0;
}
