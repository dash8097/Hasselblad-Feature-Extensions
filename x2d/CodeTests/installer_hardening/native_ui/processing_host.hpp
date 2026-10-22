#pragma once
#include "processing_stock_view.hpp"
#include "exposure_model_provider.hpp"
namespace x2dui {
struct FocusProfile {void*(*window)(const void*);void*(*active)(const void*);void(*focus)(void*,int);};
void*find_named_item(const MenuProfile&,void*root,const char16_t*name);
// The caller supplies the freshly enumerated window root every tick. Saved
// foreign focus pointers are destruction-watched and tree-validated before use.
class ProcessingHost {
 StockProfile stock;MenuProfile menu;AbiViewProfile*abi;SignalProfile signal;KeyProfile keys;KeyMapProfile map;FocusProfile focus;
 ExposureModelProvider exposureModels;
 ProcessingStockView*view=nullptr;void*scene=nullptr,*returnFocus=nullptr;SignalWatch*returnWatch=nullptr;
 void(*action)(void*,ProcessingAction);void*context;
 static void focusDestroyed(void*);static void release(void*){};
 bool enabledVisible(void*)const;
public:
 ProcessingHost(StockProfile,MenuProfile,AbiViewProfile*,SignalProfile,KeyProfile,KeyMapProfile,FocusProfile,void(*)(void*,ProcessingAction),void*);
 ~ProcessingHost();
 bool sync(void*freshRoot,bool requested,const ProcessingState&,double width,double height,double scale=1,double scaleX=1);
 void close(void*freshRoot);void*item()const{return view?view->item():nullptr;}
};
}
