#pragma once
#include "stock_factory.hpp"
#include "menu_discovery.hpp"
#include "signal_bridge.hpp"
namespace x2dui {
// Only the original ExposeScreenViewModel qrc component. Its original main
// proxy is borrowed from ControlScreen; never constructs a new camera proxy.
class ExposureModelProvider {
 StockProfile stock;MenuProfile menu;SignalProfile signals;void*scene=nullptr,*object=nullptr,*main=nullptr;bool owned=false;
 SignalWatch*objectDeath=nullptr,*mainDeath=nullptr;
 static void objectGone(void*);static void mainGone(void*);static void released(void*){}
public:
 ExposureModelProvider(StockProfile s,MenuProfile m,SignalProfile g):stock(s),menu(m),signals(g){}
 ~ExposureModelProvider(){close();}
 void close();void*resolve(void*freshRoot);
 static bool context_available(const StockProfile&,const MenuProfile&,void*freshRoot);
};
}
