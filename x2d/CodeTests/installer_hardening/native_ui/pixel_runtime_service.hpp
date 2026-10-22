#pragma once
#include "pixel_gui_runtime.hpp"
#include "factory_proxy.hpp"
#include "album_adapter.hpp"
#include "stock_metrics.hpp"
namespace x2dui {
struct PixelWorkerApi {void*context;X2dPixelIo*(*create)(void*);void(*stop)(void*,X2dPixelIo*);int(*release)(void*,X2dPixelIo*);PixelIoApi(*io)(void*,X2dPixelIo*);};
PixelWorkerApi pixel_worker_api();
// One GUI controller and one file worker for the process. The owner must keep
// this object alive until stop()+drain() completes; no blocking join on GUI.
class PixelRuntimeService {
 StockProfile stock;MenuProfile menu;AbiViewProfile*abi;SignalProfile signals;KeyProfile keys;KeyMapProfile keymap;FocusProfile focus;
 FactoryProxyProfile factory;AlbumAdapter album;PixelWorkerApi worker;
 X2dPixelIo*io=nullptr;PixelGuiRuntime*runtime=nullptr;bool stopping=false,contextReady=false;
 AlbumContext currentAlbum;
 PixelHostAction pending;bool held=false;
 SignalWatch*storageAvailableWatch=nullptr,*storageSyncWatch=nullptr,*rootDestroyWatch=nullptr,*storageDestroyWatch=nullptr;void*storage=nullptr,*receiverRoot=nullptr;
 static void available(void*,void**);static void synced(void*);static void rootDestroyed(void*);static void storageDestroyed(void*);static void released(void*){}
 bool bindStorage(void*receiver);
public:
 PixelRuntimeService(StockProfile s,MenuProfile m,AbiViewProfile*a,SignalProfile g,KeyProfile k,KeyMapProfile km,FocusProfile f,FactoryProxyProfile p,AlbumProfile ap,PixelWorkerApi w):stock(s),menu(m),abi(a),signals(g),keys(k),keymap(km),focus(f),factory(p),album(ap,s,m),worker(w){}
 ~PixelRuntimeService();
 bool tick(void*freshRoot,uint64_t now,const StockMetricsSnapshot&,double width,double height);
 PixelGuiRuntime*gui(){return runtime;}
 const AlbumContext&album_context()const{return currentAlbum;}
 bool ready_context()const{return contextReady&&!stopping;}
 // Only unhandled GUI/proxy actions are exposed. Completion acknowledges
 // acceptance by an external coordinator, not camera execution success.
 const PixelHostAction*external_action()const{return held?&pending:nullptr;}
 void accepted_external(){held=false;}
 void suspend(void*freshRoot){contextReady=false;if(runtime)runtime->close(freshRoot);}
 void stop(void*freshRoot);bool drain();
};
}
