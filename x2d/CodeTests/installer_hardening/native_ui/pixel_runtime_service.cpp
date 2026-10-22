#include "pixel_runtime_service.hpp"
#include <cstdlib>
#include <new>
namespace x2dui {
PixelWorkerApi pixel_worker_api(){return {nullptr,[](void*){return x2d_pixel_io_create();},[](void*,X2dPixelIo*p){x2d_pixel_io_stop(p);},[](void*,X2dPixelIo*p){return x2d_pixel_io_release(p);},[](void*,X2dPixelIo*p){return pixel_io_api(p);}};}
PixelRuntimeService::~PixelRuntimeService(){stop(nullptr);drain();}
void PixelRuntimeService::available(void*p,void**args){auto*s=static_cast<PixelRuntimeService*>(p);if(s->runtime&&args&&args[1])s->runtime->host().storage_available(*static_cast<bool*>(args[1]));}
void PixelRuntimeService::synced(void*p){auto*s=static_cast<PixelRuntimeService*>(p);if(s->runtime)s->runtime->host().storage_synced();}
void PixelRuntimeService::rootDestroyed(void*p){auto*s=static_cast<PixelRuntimeService*>(p);s->receiverRoot=nullptr;s->contextReady=false;}
void PixelRuntimeService::storageDestroyed(void*p){auto*s=static_cast<PixelRuntimeService*>(p);s->storage=nullptr;s->contextReady=false;}
bool PixelRuntimeService::bindStorage(void*receiver){
 void*next=factory.slots[2]?*factory.slots[2]:nullptr;if(!next||!factory.cast(factory.meta[2],next))return false;if(next==storage&&receiverRoot==receiver&&storageAvailableWatch&&storageSyncWatch)return true;
 signal_unwatch(storageAvailableWatch);signal_unwatch(storageSyncWatch);signal_unwatch(rootDestroyWatch);signal_unwatch(storageDestroyWatch);storageAvailableWatch=storageSyncWatch=rootDestroyWatch=storageDestroyWatch=nullptr;storage=receiverRoot=nullptr;
 storageAvailableWatch=signal_watch_args(signals,next,receiver,"availableChanged(bool)",available,this,released);storageSyncWatch=signal_watch(signals,next,receiver,"synced()",synced,this,released);rootDestroyWatch=signal_watch(signals,receiver,receiver,"destroyed(QObject*)",rootDestroyed,this,released);storageDestroyWatch=signal_watch(signals,next,receiver,"destroyed(QObject*)",storageDestroyed,this,released);if(!storageAvailableWatch||!storageSyncWatch||!rootDestroyWatch||!storageDestroyWatch){signal_unwatch(storageAvailableWatch);signal_unwatch(storageSyncWatch);signal_unwatch(rootDestroyWatch);signal_unwatch(storageDestroyWatch);storageAvailableWatch=storageSyncWatch=rootDestroyWatch=storageDestroyWatch=nullptr;return false;}storage=next;receiverRoot=receiver;return true;
}
bool PixelRuntimeService::tick(void*root,uint64_t now,const StockMetricsSnapshot&metrics,double width,double height){
 contextReady=false;if(stopping)return false;int64_t format=-1,browsing=1,exposureMode=-1;
 const bool modeKnown=factory_read(factory,FactoryField::ExposureMode,exposureMode);
 if(runtime)runtime->controller().set_exposure_mode(modeKnown?int(exposureMode):-1);
 if(!ExposureModelProvider::context_available(stock,menu,root)||!factory_read(factory,FactoryField::ImageFormat,format)||!factory_read(factory,FactoryField::StorageBrowsing,browsing)||!key_instance(keymap)){if(runtime)runtime->close(root);return false;}
 AlbumContext current;if(!album.snapshot(root,browsing!=0,current)){if(runtime)runtime->close(root);return false;}
 currentAlbum=current;if(!currentAlbum.path.valid()){if(runtime)runtime->close(root);return false;}
 if(!runtime){io=worker.create(worker.context);if(!io)return false;void*memory=std::malloc(sizeof(PixelGuiRuntime));if(!memory){worker.stop(worker.context,io);stopping=true;return false;}runtime=new(memory)PixelGuiRuntime(stock,menu,abi,signals,keys,keymap,focus,worker.io(worker.context,io));}
 if(!bindStorage(root)){runtime->close(root);return false;}
 runtime->controller().set_image_format(int(format));runtime->controller().set_exposure_mode(modeKnown?int(exposureMode):-1);if(!runtime->tick(root,now,current,width,height,metrics.scale,metrics.scaleX,metrics.scaleY))return false;contextReady=true;
 for(unsigned n=0;n<16&&!held;n++){if(!runtime->take_action(pending))break;held=true;switch(pending.kind){
 // page.sync 已在上面完成页面退出及焦点归还；这个通知不属于原厂代理写入。
 // 若留给只处理 ClosePopup/SetStockSingle 的 DriveIntegration，会永久阻塞队列。
 case PixelHostActionKind::FocusReport:held=false;break;
 case PixelHostActionKind::RefreshAlbumSamePath:case PixelHostActionKind::EnterAlbum:if(!album.dispatch(pending,root,browsing!=0))runtime->host().queue_album_refresh(pending.path);held=false;break;case PixelHostActionKind::UpdateAlbumLoading:album.dispatch(pending,root,browsing!=0);held=false;break;default:break;}}
 return true;
}
void PixelRuntimeService::stop(void*root){if(stopping&& !runtime)return;stopping=true;contextReady=false;signal_unwatch(storageAvailableWatch);signal_unwatch(storageSyncWatch);signal_unwatch(rootDestroyWatch);signal_unwatch(storageDestroyWatch);storageAvailableWatch=storageSyncWatch=rootDestroyWatch=storageDestroyWatch=nullptr;storage=receiverRoot=nullptr;if(runtime){runtime->close(root);runtime->~PixelGuiRuntime();std::free(runtime);runtime=nullptr;}held=false;if(io)worker.stop(worker.context,io);}
bool PixelRuntimeService::drain(){if(!stopping)return false;if(!io)return true;if(!worker.release(worker.context,io))return false;io=nullptr;return true;}
}
