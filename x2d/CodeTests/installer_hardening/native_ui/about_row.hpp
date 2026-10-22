#pragma once
#include "about_display.hpp"
#include "factory_formatter.hpp"
#include "stock_factory.hpp"
#include "signal_bridge.hpp"
#include "binding_lease.hpp"
namespace x2dui {
// The attachment supplies a positively identified about-page, right-anchored
// TextDelegate label. Lifetime signals own every borrowed pointer.
class AboutRow final:private LocalHeap {
 StockProfile stock;MenuProfile menu;SignalProfile signal;FactoryFormatterProfile formatter;BindingLease lease;
 void*model=nullptr,*label=nullptr;SignalWatch*watches[6]{};UiText name,original,rendered;
 int refs=1;bool closed=false,updating=false;
 AboutRow(StockProfile s,MenuProfile m,SignalProfile g,FactoryFormatterProfile f):stock(s),menu(m),signal(g),formatter(f){}
 void retain(){++refs;}void drop(){if(!--refs)delete this;}
 static void release(void*);static void changed(void*);static void model_gone(void*);static void label_gone(void*);static void formatter_gone(void*);
 bool watch(unsigned,void*,const char*,void(*)(void*));bool text(void*,const char*,UiText&);bool formatted(UiText&);
public:
 static AboutRow*create(StockProfile,MenuProfile,SignalProfile,FactoryFormatterProfile,BindingProfile,void*model,void*label,bool aboutPage,bool rightAnchored);
 bool refresh();void close();
};
}
