#pragma once
#include "factory_proxy.hpp"
#include "factory_formatter.hpp"
#include "stock_factory.hpp"
#include "screen_controls.hpp"
namespace x2dui {
struct ScreenFactoryProfile {
 FactoryProxyProfile proxy;FactoryFormatterProfile formatter;
 void(*list_destroy)(void*)=nullptr;
 unsigned listMethod=4,setMethod=5,displayMethod=6,translateMethod=9,fpsIndex=11;
 int backBit=0,evfBit=0,halfBit=0,systemProxy=-1,cameraProxy=-1;
};
struct ScreenFactorySnapshot {
 char16_t labels[64][128]{},rawLabels[64][128]{},current[128]{},zero[128]{};
 const char16_t*labelPointers[64]{};ScreenControlSnapshot state;
};
// 固定三种字段；不接受任意属性名/代理编号，不调用单例创建入口。
bool screen_factory_snapshot(const ScreenFactoryProfile&,const StockProfile&,ScreenControlKind,bool enabled,ScreenFactorySnapshot&);
bool screen_factory_write(const ScreenFactoryProfile&,const StockProfile&,ScreenControlKind,const ScreenWrite&);
}
