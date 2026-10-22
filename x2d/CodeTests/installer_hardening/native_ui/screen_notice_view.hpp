#pragma once
#include "stock_factory.hpp"
namespace x2dui {
// 复用原厂PopupIconText，只有提示显示，不创建鼠标区域、不转发快门。
class ScreenNoticeView final:private LocalHeap {
 ViewApi api_;StockProfile stock_;void*object_=nullptr;int refs_=1;bool closed_=false;
 ScreenNoticeView(ViewApi a,StockProfile p):api_(a),stock_(p){}
 static void event(void*,Event,double);static void released(void*);void drop(){if(!--refs_)delete this;}
public:
 static ScreenNoticeView*create(ViewApi,StockProfile,void*contextObject,void*overlay);
 bool update(bool show,double width,double height,const char16_t*translatedText);
 void close();void*item()const{return object_;}
};
}
