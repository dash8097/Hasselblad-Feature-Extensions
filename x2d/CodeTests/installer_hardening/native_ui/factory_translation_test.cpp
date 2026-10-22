#include "factory_translation.hpp"
#include <QCoreApplication>
#include <QTranslator>
#include <QtCore/qarraydata.h>
#include <cstdio>
#include <cstring>
using namespace x2dui;
static unsigned checks;
#define C(x) do{++checks;if(!(x)){printf("TRANSLATION_FAIL line=%d\n",__LINE__);return 1;}}while(0)
class Translator:public QTranslator{public:QString value=QString::fromUtf16(u"仅在取景器中显示实时取景");QString translate(const char*c,const char*s,const char*d,int n)const override {return !strcmp(c,"TimeoutText")&&!strcmp(s,"Live View in EVF Only")&&!d&&n==-1?value:QString();}bool isEmpty()const override{return false;}};
int main(int argc,char**argv){QCoreApplication app(argc,argv);TranslationProfile p;p.translate=reinterpret_cast<decltype(p.translate)>(&QCoreApplication::translate);p.deallocate=[](void*a,std::ptrdiff_t s,std::ptrdiff_t al){QArrayData::deallocate(static_cast<QArrayData*>(a),s,al);};auto en=factory_screen_notice(p);C(en.valid()&&en==u"Live View in EVF Only");Translator t;C(app.installTranslator(&t));for(unsigned i=0;i<256;i++){t.value=QString::fromUtf16(i%2?u"取景器提示":u"Nur im Sucher");auto x=factory_screen_notice(p);C(x.valid()&&x==(i%2?u"取景器提示":u"Nur im Sucher"));t.value.fill(QChar('X'));C(x==(i%2?u"取景器提示":u"Nur im Sucher"));}t.value=QString(512,'x');C(!factory_screen_notice(p).valid());t.value=QString::fromUtf16(u"A\0B",3);C(!factory_screen_notice(p).valid());C(app.removeTranslator(&t));C(factory_screen_notice(p)==u"Live View in EVF Only");C(factory_liveview_title(8)==u"取景超时");C(factory_liveview_title(9)==u"取景逾時");C(factory_liveview_title(0)==u"Live view timeout");for(unsigned i=0;i<11;++i)C(factory_liveview_title(i).valid());const char16_t*choices[]={u"Never",u"10 s",u"20 s",u"30 s",u"1 m",u"2 m",u"5 m"};for(unsigned i=0;i<7;++i)C(factory_liveview_choice(p,i)==choices[i]);C(!factory_liveview_choice(p,7).valid());p.translate=nullptr;C(!factory_screen_notice(p).valid());printf("TRANSLATION_PASS assertions=%u actual_Qt_translate=1 camera_operations=0\n",checks);}
