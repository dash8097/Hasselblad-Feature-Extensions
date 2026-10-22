#pragma once
#include "stock_factory.hpp"
#include "signal_bridge.hpp"
#include "processing_presentation.hpp"
#include "key_mapping.hpp"
namespace x2dui {
enum class ProcessingAction {Close,Finish};
// All business state is native; the three visual components are original
// resources already owned by the running camera GUI, never bundled here.
class ProcessingStockView final:private LocalHeap {
 StockProfile stock;MenuProfile menu;AbiViewProfile*abi;ViewApi view;
 void*root=nullptr,*progress=nullptr,*countdown=nullptr,*model=nullptr,*button=nullptr,*touch=nullptr,*backdrop=nullptr;
 void*fields[3]{},*countText=nullptr,*area=nullptr;SignalWatch*abortWatch=nullptr,*modelWatch=nullptr;
 NativeKeyFilter*keys=nullptr;KeyMapProfile keyMap{};
 int refs=1;bool closed=false,available=false,finish=false;double width=0,height=0;
 void(*action)(void*,ProcessingAction)=nullptr;void*context=nullptr;
 ProcessingStockView(StockProfile s,MenuProfile m,AbiViewProfile*a):stock(s),menu(m),abi(a),view(abi_view_api(a)){}
 ~ProcessingStockView()=default;void retain(){++refs;}void drop(){if(!--refs)delete this;}
 static void release(void*);static void event(void*,Event,double);static void aborted(void*);static void modelDestroyed(void*);
 static bool key(void*,const KeyInput&);
 bool bind(void*,Event);void invoke();void*findArea();bool layout(double,double);
public:
 static ProcessingStockView*create(StockProfile,MenuProfile,AbiViewProfile*,SignalProfile,void*contextObject,void*parent,void*factoryExposureModel,void(*)(void*,ProcessingAction),void*context,const KeyProfile*keyProfile=nullptr,const KeyMapProfile*keyMap=nullptr);
 bool update(const ProcessingState&,double width,double height,double scale=1,double scaleX=1);
 void close();void*item()const{return root;}
};
}
