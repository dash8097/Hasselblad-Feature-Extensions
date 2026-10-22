#pragma once
#include "stock_factory.hpp"
#include "multishot_settings_view.hpp"
#include "drive_popup_view.hpp"
namespace x2dui {
struct StockMetricsSnapshot {double scale=0,scaleX=0,scaleY=0;SettingsStyle settings;DriveStyle drive;};
// Read existing Constants bindings via hidden original presentation controls.
// No qml expression evaluation, new singleton, or guessed screen-width ratio.
class StockMetrics {
 StockProfile stock;MenuProfile menu;ViewApi api;void*root=nullptr,*host=nullptr,*row=nullptr,*header=nullptr;
 static void event(void*,Event,double);static void released(void*){}
 bool attach(void*);bool number(void*,const char*,double&)const;unsigned color(void*,const char*,unsigned)const;
public:
 StockMetrics(StockProfile s,MenuProfile m,AbiViewProfile*a):stock(s),menu(m),api(abi_view_api(a)){}
 ~StockMetrics(){close();}
 bool read(void*freshRoot,bool external,StockMetricsSnapshot&);
 void close();void*item()const{return host;}
};
}
