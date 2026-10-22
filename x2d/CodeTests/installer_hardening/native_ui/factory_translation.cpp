#include "factory_translation.hpp"
namespace x2dui {
UiText factory_liveview_title(unsigned language){
 // HblmTypes::E_Language，第一代官方4.2.0元对象核对。
 static const char16_t*titles[]={u"Live view timeout",u"Tiempo de vista en vivo",u"Délai de visée",u"Live-View-Zeitlimit",u"Tempo Live View",u"Tidsgräns för direktvisning",u"Время визирования",u"ライブビュー時間",u"取景超时",u"取景逾時",u"라이브 뷰 시간 제한"};
 return UiText(titles[language<11?language:0]);
}
UiText factory_liveview_choice(const TranslationProfile&p,unsigned index){
 static const char*values[]={"Never","10 s","20 s","30 s","1 m","2 m","5 m"};UiText result;
 if(index>=7||!p.translate||!p.deallocate){result.invalidate();return result;}
 auto a=p.translate("IdleTimeout",values[index],nullptr,-1);
 if(a.size<=0||a.size>127||!a.data)result.invalidate();else{auto*t=static_cast<const char16_t*>(a.data);bool valid=true;for(std::ptrdiff_t i=0;i<a.size;i++)if(!t[i])valid=false;if(valid)result=UiText(t,size_t(a.size));else result.invalidate();}
 if(a.owner){auto*r=static_cast<int*>(a.owner);if(__atomic_load_n(r,__ATOMIC_RELAXED)>=0&&__atomic_sub_fetch(r,1,__ATOMIC_ACQ_REL)==0)p.deallocate(a.owner,2,8);}return result;
}
UiText factory_screen_notice(const TranslationProfile&p){
 UiText result;
 if(!p.translate||!p.deallocate){result.invalidate();return result;}
 auto a=p.translate("TimeoutText","Live View in EVF Only",nullptr,-1);
 if(a.size<=0||a.size>511||!a.data)result.invalidate();
 else {auto*t=static_cast<const char16_t*>(a.data);bool valid=true;for(std::ptrdiff_t i=0;i<a.size;i++)if(!t[i])valid=false;if(valid)result=UiText(t,size_t(a.size));else result.invalidate();}
 if(a.owner){auto*r=static_cast<int*>(a.owner);if(__atomic_load_n(r,__ATOMIC_RELAXED)>=0&&__atomic_sub_fetch(r,1,__ATOMIC_ACQ_REL)==0)p.deallocate(a.owner,2,8);}
 return result;
}
}
