#include "liveview_preferences.hpp"
#ifdef X2D_UI_PRODUCTION
#define X2D_UI_INTERNAL_API __attribute__((visibility("hidden")))
#else
#define X2D_UI_INTERNAL_API __attribute__((visibility("default")))
#endif
#include "abi_view_api.hpp"
#include "effect_menu_attachment.hpp"
#include "effect_service.h"
#include "tree_observer.hpp"
#include "playback_attachment.hpp"
#include "pixel_runtime_service.hpp"
#include "bridge_core.h"
#include "album_profile.hpp"
#include "factory_proxy_profile.hpp"
#include "object_profile.hpp"
#include "formatter_profile.hpp"
#include "about_profile.hpp"
#include "drive_profile.hpp"
#include "key_profile.hpp"
#include "screen_profile.hpp"
#include "screen_keys.hpp"
#include "screen_live.hpp"
#include "screen_attachment.hpp"
#include "drive_proxy_adapter.hpp"
#include "animation_profile.hpp"
#include "animation_attachment.hpp"
#include "face_popup_attachment.hpp"
#include "face_live_attachment.hpp"
#include "face_factory_profile.hpp"
#include "face_live_generated.hpp"
#include "face_style_profile.hpp"
#include "factory_translation.hpp"
#include "translation_profile.hpp"
#include "face_popup_profile.hpp"
#include "focus_attachment.hpp"
#include "focus_profile.hpp"
#include "playback_sampler.hpp"
#include "passive_output.h"
#include "ui_receipt.h"
#include "native_feature_contract.h"
#include "native_startup_guard.h"
#include "stock_profile.hpp"
#include "binding_profile.hpp"
#include <new>
#include <cstdlib>
#include <time.h>
#include <cstdio>
extern "C" int x2d_ui_runtime_image(UiImage*);
extern "C" int x2d_ui_verified_image(UiImage*);
// Our controller allocation remains local; original QObject allocations use
// the original GUI operator-new PLT entry. No second Qt or C++ runtime is linked.
struct ViewHandle:x2dui::LocalHeap {x2dui::AbiViewProfile api; x2dui::EffectView*view;};
static bool profile_valid(const UiImage&image){
 if(!ui_code(&image,&translation_function))return false;
 for(const auto&code:object_functions)if(!ui_code(&image,&code))return false;
 for(const auto&data:object_data)if(!ui_range(&image,data.address,data.size,PF_R))return false;
 for(const auto&code:stock_functions)if(!ui_code(&image,&code))return false;
 for(const auto&data:stock_data)if(!ui_range(&image,data.address,data.size,PF_R))return false;
 for(const auto&code:formatter_functions)if(!ui_code(&image,&code))return false;
 for(const auto&data:formatter_data)if(!ui_range(&image,data.address,data.size,PF_R))return false;
 for(const auto&data:face_live_data)if(!ui_range(&image,data.address,data.size,PF_R))return false;
 for(const auto&code:face_live_functions)if(!ui_code(&image,&code))return false;
 for(const auto&code:face_style_functions)if(!ui_code(&image,&code))return false;
 for(const auto&code:face_factory_functions)if(!ui_code(&image,&code))return false;
 if(!ui_range(&image,face_popup_list_type,56,PF_R))return false;
 for(const auto&code:animation_image_functions)if(!ui_code(&image,&code))return false;
 for(const auto&code:screen_functions)if(!ui_code(&image,&code))return false;
 for(const auto&code:screen_key_functions)if(!ui_code(&image,&code))return false;
 for(const auto&code:screen_live_functions)if(!ui_code(&image,&code))return false;
 for(const auto&code:drive_attachment_functions)if(!ui_code(&image,&code))return false;
 for(const auto&code:about_functions)if(!ui_code(&image,&code))return false;
 for(const auto&code:album_functions)if(!ui_code(&image,&code))return false;
 if(!ui_range(&image,album_slot,8,PF_R)||!ui_range(&image,album_meta,56,PF_R))return false;
 for(const auto&code:factory_proxy_functions)if(!ui_code(&image,&code))return false;
 for(const auto&slot:factory_proxy_slots)if(!ui_range(&image,slot,8,PF_R))return false;
 for(const auto&meta:factory_proxy_metas)if(!ui_range(&image,meta,56,PF_R))return false;
 for(const auto&code:binding_functions)if(!ui_code(&image,&code))return false;
 for(const auto&code:key_functions)if(!ui_code(&image,&code))return false;
 if(!ui_range(&image,key_slot,8,PF_R)||!ui_range(&image,key_event_meta,56,PF_R)||!ui_range(&image,key_handler_meta,56,PF_R))return false;
 return true;
}
extern "C" X2D_UI_INTERNAL_API void*x2d_native_effect_view_create_v1(void*host,void*overlay,void(*choose)(void*,int),void*context){
 UiImage image{};if(x2d_ui_runtime_image(&image)||!profile_valid(image)||!host||!overlay)return nullptr;
 auto cast=reinterpret_cast<const void*(*)(const void*,const void*)>(image.base+object_cast);
 if(!cast(reinterpret_cast<void*>(image.base+object_meta_item),host)||!cast(reinterpret_cast<void*>(image.base+object_meta_item),overlay))return nullptr;
 auto*h=new(std::nothrow)ViewHandle{};if(!h)return nullptr;h->api=object_profile(image.base);
 h->view=x2dui::EffectView::create(x2dui::abi_view_api(&h->api),host,overlay,choose,context);
 if(!h->view){delete h;return nullptr;}return h;
}
extern "C" X2D_UI_INTERNAL_API int x2d_native_effect_view_update_v1(void*handle,int mode,int available,double width,double height){
 if(!handle||available<0||available>1||mode<0||mode>2||!(width>0&&width<=8192&&height>0&&height<=8192))return 1;
 UiImage image{};if(x2d_ui_runtime_image(&image))return 2;auto*h=static_cast<ViewHandle*>(handle);h->view->update(mode,available!=0);h->view->resize(width,height);return 0;
}
extern "C" X2D_UI_INTERNAL_API int x2d_native_effect_view_close_v1(void*handle){
 if(!handle)return 1;UiImage image{};if(x2d_ui_runtime_image(&image))return 2;auto*h=static_cast<ViewHandle*>(handle);h->view->close();delete h;return 0;
}

extern "C" int x2d_ui_recheck_image(const UiImage*);
extern "C" int x2d_ui_schedule_image(const UiImage*,int,void(*)(void*),void(*)(void*),void*);
extern "C" int x2d_native_effect_menu_tick_v1(void*,int,uint64_t);
static void array_drop(const x2dui::MenuProfile&p,x2dui::AbiArray&a){
 if(a.owner){auto*ref=static_cast<int*>(a.owner);if(__atomic_load_n(ref,__ATOMIC_RELAXED)>=0&&__atomic_sub_fetch(ref,1,__ATOMIC_ACQ_REL)==0)p.deallocate(a.owner,8,8);}
}
static void stock_object_destroy(void*);
// Bounded in-memory timing only; the existing worker publishes the report.
struct UiTickTiming {
 uint64_t maximum[16]{},count=0,lastStart=0,gapMax=0;
 static uint64_t clock(){timespec t{};return !clock_gettime(CLOCK_MONOTONIC,&t)&&t.tv_sec>=0?uint64_t(t.tv_sec)*1000000+uint64_t(t.tv_nsec)/1000:0;}
 void add(unsigned stage,uint64_t start){uint64_t end=clock();if(stage<16&&start&&end>=start&&end-start>maximum[stage])maximum[stage]=end-start;}
 struct Scope{UiTickTiming&owner;unsigned stage;uint64_t start;Scope(UiTickTiming&o,unsigned s):owner(o),stage(s),start(clock()){}~Scope(){owner.add(stage,start);}};
 void begin(){uint64_t at=clock();if(lastStart&&at>=lastStart&&at-lastStart>gapMax)gapMax=at-lastStart;lastStart=at;++count;}
};
struct DriveIntegration:x2dui::LocalHeap {
 x2dui::DriveSettingsAttachment view;x2dui::DriveProxyAdapter proxy;UiTickTiming timing;void*scene=nullptr;
 DriveIntegration(x2dui::StockProfile s,x2dui::MenuProfile m,x2dui::AbiViewProfile*a,x2dui::SignalProfile g,x2dui::BindingProfile b,x2dui::KeyProfile k,x2dui::KeyMapProfile km,x2dui::FocusProfile f,x2dui::DriveAttachmentProfile d,x2dui::FactoryProxyProfile fp,x2dui::PixelGuiRuntime&r):view(s,m,a,g,b,k,km,f,d,r.controller(),r.host()),proxy(fp,r.controller(),d.drive){}
 void close(){proxy.cancel();view.close();scene=nullptr;}
 void tick(void*root,uint64_t now,x2dui::PixelRuntimeService&p,const x2dui::StockMetricsSnapshot&m,double w,double h){
  if(scene!=root){close();scene=root;}{UiTickTiming::Scope t(timing,0);view.tick(root,now,p.album_context(),m.drive,m.settings,w,h);}
  {UiTickTiming::Scope t(timing,3);if(auto*action=p.external_action()){if(view.dispatch_host_action(*action,now))p.accepted_external();}}
  {UiTickTiming::Scope t(timing,1);proxy.tick(now);uint64_t id=0;x2dui::DriveSessionResult result;if(proxy.take_result(id,result))view.complete(id,result,now);}
  {UiTickTiming::Scope t(timing,2);if(!proxy.active()){x2dui::DriveSessionIntent i;if(view.take_intent(i,now)&&!proxy.begin(i,now))view.complete(i.id,x2dui::DriveSessionResult::Rejected,now);}}
 }
};
struct MenuSession: x2dui::LocalHeap {
 x2dui::LiveviewPreferences liveviewPreferences;int liveviewLanguage=-1;
 static bool readLiveview(void*p,int&seconds,bool&confirmed){auto&s=*static_cast<MenuSession*>(p);seconds=s.liveviewPreferences.seconds();confirmed=s.liveviewPreferences.ready();return confirmed||s.liveviewPreferences.saving()||s.liveviewPreferences.error();}
 static bool writeLiveview(void*p,int seconds,uint64_t now){return static_cast<MenuSession*>(p)->liveviewPreferences.save(seconds,now);}
 X2dFlashClient flashClient{};x2dui::FlashExposure observedExposure{};const unsigned featureMask=x2d_native_feature_mask(std::getenv(X2D_NATIVE_UI_FEATURE_ENV));UiTickTiming timing;UiImage image;x2dui::AbiViewProfile api;x2dui::MenuProfile menu;X2dEffectService service;uint64_t now=0,lastObservation=0;int refs=1,lastResult=1;bool running=false,closed=false;
 x2dui::TreeObserver observer;x2dui::EffectMenuAttachment attachment;x2dui::PlaybackAttachment playback;x2dui::StockMetrics metrics;x2dui::PixelRuntimeService pixel;x2dui::AboutAttachment about;x2dui::PlaybackSampler sampler;X2dPassiveOutput*passive=nullptr;DriveIntegration*drive=nullptr;x2dui::ScreenAttachment*screen=nullptr;x2dui::ScreenModeAttachment*screenMode=nullptr;x2dui::AnimationAttachment animation;x2dui::FacePopupAttachment*facePopup=nullptr;void*faceRoot=nullptr;x2dui::FaceLiveAttachment*faceLive=nullptr;bool faceClosing=false;x2dui::FocusAttachment*focusPopup=nullptr;void*focusRoot=nullptr;X2dUiReceiptWorker*receipt=nullptr;void*receiptRoot=nullptr;void*capRoot=nullptr;void*screenRoot=nullptr;int screenLanguage=-1;bool effectHooks=false,aboutHooks=false;
 static void select(void*context,int mode){auto*s=static_cast<MenuSession*>(context);x2d_effect_select(&s->service,mode,s->now);}
 static bool validate_vtable(void*opaque,const void*table,const void*fn){auto*i=static_cast<UiImage*>(opaque);auto t=reinterpret_cast<uintptr_t>(table),f=reinterpret_cast<uintptr_t>(fn);return t>=i->base&&f>=i->base&&ui_range(i,t-i->base,sizeof(void*),PF_R)&&ui_range(i,f-i->base,4,PF_R|PF_X);}
 x2dui::SignalProfile signals(){auto p=signal_profile(image.base);p.valid_vtable=validate_vtable;p.validation_context=&image;return p;}
 x2dui::StockProfile stock(){auto p=stock_profile(image.base);p.valid_vtable=validate_vtable;p.validation_context=&image;p.object_destroy=stock_object_destroy;return p;}
 explicit MenuSession(UiImage i):image(i),api(object_profile(i.base)),menu(menu_profile(i.base)),observer(menu,x2dui::abi_view_api(&api),signals()),attachment(menu,x2dui::abi_view_api(&api),select,this,stock(),binding_profile(image.base),signals()),playback(stock(),menu,&api,binding_profile(image.base),signals()),metrics(stock(),menu,&api),pixel(stock(),menu,&api,signals(),key_profile(image.base),key_map_profile(image.base),focus_profile(image.base),factory_proxy_profile(image.base),album_profile(image.base),x2dui::pixel_worker_api()),about(stock(),menu,x2dui::abi_view_api(&api),signals(),formatter_profile(image.base),binding_profile(image.base),about_profile(image.base)),sampler(stock(),menu,&api,signals()),animation(x2dui::abi_view_api(&api),stock(),menu,signals(),animation_image_profile(image.base),&service,{this,[](void*)->uint64_t{timespec t{};if(clock_gettime(CLOCK_MONOTONIC,&t)||t.tv_sec<0)return 0;return uint64_t(t.tv_sec)*1000+uint64_t(t.tv_nsec)/1000000;}}){attachment.configure_keys(screen_key_profile(image.base),key_map_profile(image.base));x2d_flash_client_init(&flashClient);x2d_effect_init(&service);passive=x2d_passive_create();receipt=x2d_ui_receipt_create_loaded("16391452abdc69de9e0807e065c0f4ab3f1ccb5fc288f6fc4e6f5cb3bdca12e0");}
 ~MenuSession(){x2d_flash_client_close(&flashClient);delete drive;closeScreen();closeFace();closeFocus();}
 void closeFocus(){focusRoot=nullptr;if(focusPopup){focusPopup->~FocusAttachment();std::free(focusPopup);focusPopup=nullptr;}}
 void focusTick(void*root,const x2dui::StockMetricsSnapshot&m,double w,double h){
  if(focusRoot!=root){closeFocus();focusRoot=root;}
  if(!focusPopup){x2dui::FocusPopupStyle style;style.scale=m.scale;style.scaleX=m.drive.scaleX;style.scaleY=m.drive.scaleY;style.headerHeight=m.settings.headerHeight;style.foreground=m.settings.foreground;void*mem=std::malloc(sizeof(x2dui::FocusAttachment));if(mem){focusPopup=new(mem)x2dui::FocusAttachment(x2dui::abi_view_api(&api),stock(),menu,binding_profile(image.base),signals(),focus_factory_profile(image.base),face_popup_profile(image.base),style);if(!focusPopup->configure_keys(screen_key_profile(image.base),key_map_profile(image.base)))closeFocus();}}
  int64_t active=0;bool touch=factory_read(factory_proxy_profile(image.base),x2dui::FactoryField::ScreensActive,active)&&active!=2;if(focusPopup){auto*g=pixel.gui();const auto*state=g?&g->controller().state():nullptr;int extension=state&&state->applied?(state->stackMode?2:1):0;focusPopup->tick(root,now,w,h,touch,extension);}
 }
 bool drainFace(){if(!faceLive)return true;if(!faceClosing||!faceLive->drain())return false;faceLive->~FaceLiveAttachment();std::free(faceLive);faceLive=nullptr;faceClosing=false;return true;}
 void closeFace(){faceRoot=nullptr;if(faceLive){faceClosing=true;faceLive->close();drainFace();}if(facePopup){facePopup->~FacePopupAttachment();std::free(facePopup);facePopup=nullptr;}}
 void faceTick(void*root,const x2dui::StockMetricsSnapshot&m,double w,double h){
  if(faceRoot!=root){closeFace();faceRoot=root;}
  if(faceClosing&&!drainFace())return;
  if(!faceLive){auto*hints=reinterpret_cast<void*(*)()>(image.base+face_style_functions[0].address)();int drag=hints?reinterpret_cast<int(*)(const void*)>(image.base+face_style_functions[1].address)(hints):-1;
   if(drag>=0&&drag<=1024){void*mem=std::malloc(sizeof(x2dui::FaceLiveAttachment));if(mem){auto*io=x2d_face_io_create();if(io)faceLive=new(mem)x2dui::FaceLiveAttachment(x2dui::abi_view_api(&api),&api,stock(),menu,binding_profile(image.base),signals(),face_live_profile(image.base),screen_key_profile(image.base),key_map_profile(image.base),io,double(drag));else std::free(mem);}}
  }
  if(faceLive)faceLive->tick(root,now);
  if(!facePopup){x2dui::FacePopupStyle style;style.scale=m.scale;style.scaleX=m.drive.scaleX;style.scaleY=m.drive.scaleY;style.headerHeight=m.settings.headerHeight;style.foreground=m.settings.foreground;void*mem=std::malloc(sizeof(x2dui::FacePopupAttachment));if(mem){facePopup=new(mem)x2dui::FacePopupAttachment(x2dui::abi_view_api(&api),stock(),menu,binding_profile(image.base),signals(),face_factory_profile(image.base),face_popup_profile(image.base),style);if(!facePopup->configure_keys(screen_key_profile(image.base),key_map_profile(image.base)))closeFace();}}
  int64_t active=0;bool touch=factory_read(factory_proxy_profile(image.base),x2dui::FactoryField::ScreensActive,active)&&active!=2;if(facePopup)facePopup->tick(root,now,w,h,touch);
 }
 void closeScreen(){liveviewLanguage=-1;screenRoot=nullptr;screenLanguage=-1;if(screen){screen->~ScreenAttachment();std::free(screen);screen=nullptr;}if(screenMode){screenMode->~ScreenModeAttachment();std::free(screenMode);screenMode=nullptr;}}
 void screenTick(void*root,const x2dui::StockMetricsSnapshot&m,double w,double h){
  if(screenRoot!=root){closeScreen();screenRoot=root;}
  if(!screen){x2dui::ScreenControlStyle style;style.rowHeight=m.settings.rowHeight;style.left=m.settings.leftMargin;style.right=m.settings.rightMargin;style.labelFont=m.settings.labelFont;style.valueFont=m.settings.valueFont;style.descriptionFont=32*m.scale;style.selectorLeft=m.settings.selectorLeft;style.selectorRight=m.settings.selectorRight;style.selectorTop=m.settings.selectorTop;style.selectorBottom=m.settings.selectorBottom;style.visibleChoices=m.settings.visibleChoices;style.background=m.settings.background;style.foreground=m.settings.foreground;style.highlight=m.settings.highlight;style.border=m.settings.border;
   void*mem=std::malloc(sizeof(x2dui::ScreenAttachment));if(mem){screen=new(mem)x2dui::ScreenAttachment(x2dui::abi_view_api(&api),stock(),&api,menu,binding_profile(image.base),signals(),screen_factory_profile(image.base),style);if(!screen->configure_keys(screen_key_profile(image.base),key_map_profile(image.base))){screen->~ScreenAttachment();std::free(screen);screen=nullptr;}}}
  auto langProfile=focus_factory_profile(image.base);const auto&proxy=langProfile.common.proxy;void*sys=proxy.slots[1]?*proxy.slots[1]:nullptr;uint32_t language=0;int status=-1,flags=0;void*la[]={&language,nullptr,&status,&flags};if(sys&&proxy.cast(proxy.meta[1],sys))proxy.call[1](sys,1,int(langProfile.languageIndex),la);else language=UINT32_MAX;
  if(language>255){if(screenMode){screenMode->~ScreenModeAttachment();std::free(screenMode);screenMode=nullptr;}screenLanguage=-1;}
  else if(!screenMode||int(language)!=screenLanguage){x2dui::TranslationProfile tp;tp.translate=reinterpret_cast<decltype(tp.translate)>(image.base+translation_function.address);tp.deallocate=menu.deallocate;auto notice=x2dui::factory_screen_notice(tp);
   if(notice.valid()&&!notice.empty()){if(!screenMode){void*mem=std::malloc(sizeof(x2dui::ScreenModeAttachment));if(mem)screenMode=new(mem)x2dui::ScreenModeAttachment(x2dui::abi_view_api(&api),stock(),menu,signals(),screen_factory_profile(image.base),screen_key_profile(image.base),screen_live_profile(image.base),key_map_profile(image.base),notice.data());}if(screenMode&&screenMode->set_notice(notice.data()))screenLanguage=int(language);}
  }
  liveviewPreferences.tick(now);
  if(screen&&language<=10&&int(language)!=liveviewLanguage){x2dui::TranslationProfile tp;tp.translate=reinterpret_cast<decltype(tp.translate)>(image.base+translation_function.address);tp.deallocate=menu.deallocate;
   auto title=x2dui::factory_liveview_title(language);x2dui::UiText values[7];const char16_t*labels[7];bool valid=title.valid();for(unsigned i=0;i<7;i++){values[i]=x2dui::factory_liveview_choice(tp,i);valid=valid&&values[i].valid()&&!values[i].empty();labels[i]=values[i].data();}
   if(valid&&screen->configure_liveview({this,readLiveview,writeLiveview},title.data(),labels))liveviewLanguage=int(language);
  }
  int64_t active=0;bool touch=factory_read(factory_proxy_profile(image.base),x2dui::FactoryField::ScreensActive,active)&&active!=2;
  if(screen)screen->tick(root,now,w,h,touch);if(screenMode){bool idle=!x2d_native_features_advanced(featureMask);
   if(auto*r=pixel.gui()){const auto&v=r->controller().state();idle=v.initialized&&v.ready&&!v.busy&&!v.sending&&!v.preparingMode&&!v.normalSwitchPending&&(!v.applied||v.quarter_mode());}
   screenMode->configure_timeout(liveviewPreferences.seconds(),screen?screen->drawer():nullptr,idle);screenMode->tick(root,now,w,h);
  }
 }
 void closeDrive(){if(drive)drive->close();}
 void driveTick(void*root,const x2dui::StockMetricsSnapshot&m,double w,double h){if(!pixel.ready_context()||!pixel.gui()){closeDrive();return;}if(!drive)drive=new(std::nothrow)DriveIntegration(stock(),menu,&api,signals(),binding_profile(image.base),key_profile(image.base),key_map_profile(image.base),focus_profile(image.base),drive_attachment_profile(image.base),factory_proxy_profile(image.base),*pixel.gui());if(drive)drive->tick(root,now,pixel,m,w,h);}
 // Capability bits mean installed automatic handlers, not every optional page
 // already opened. Each required service must have current verified routes;
 // partial candidates can only answer ATTACHING.
 uint64_t capabilities(void*root)const {if(!root||closed)return 0;uint64_t mask=16;if(effectHooks)mask|=1;if(aboutHooks)mask|=64;if(pixel.ready_context())mask|=2|8;if(drive)mask|=4;if(screen&&screenMode&&screen->hooks_ready()&&screenMode->hooks_ready())mask|=32;if(animation.has_source())mask|=128;if(facePopup&&facePopup->has_source()&&faceLive&&!faceClosing&&faceLive->hooks_ready())mask|=256;if(focusPopup&&focusPopup->has_source())mask|=512;if(passive)mask|=1024;if(receipt)mask|=2048;return mask;}
 void receiptTick(void*root){if(!receipt)return;if(receiptRoot!=root){x2d_ui_receipt_invalidate(receipt);receiptRoot=root;}X2dUiQuery q{};if(x2d_ui_receipt_take_query(receipt,&q)){uint64_t caps=capabilities(root);x2d_ui_receipt_reply(receipt,&q,closed?X2D_UI_STOPPED:((caps&x2d_native_required_capabilities(featureMask))==x2d_native_required_capabilities(featureMask)?X2D_UI_READY:X2D_UI_ATTACHING),caps,now);}}
 void observeUi(){if(!passive||now<lastObservation||now-lastObservation<500||now>UINT64_MAX-2000)return;lastObservation=now;auto*r=pixel.gui();bool page=r&&r->processing_item();char report[2048];int n=std::snprintf(report,sizeof(report),"X2D_NATIVE_UI_OBSERVATION_V1 rawStackUI=3 popup=%u page=%u pageStatus=%u popupComponent=-1 pageCreated=%u routeVerified=0 parametersVerified=0 selectorVerified=0 iconVerified=0 factoryModel=%u\ndriveHooks=%u driveError=%u screenRows=%u screenMode=%u contextReady=%u selected_mask=%u sampled_ms=%llu\nfaceLive=%u facePopup=%u faceReady=%u faceTries=%u faceFound=%u faceStage=%u faceSnapshot=%u faceRc=%d faceFailures=%u\nfocusRc=%d focusFailures=%u focusAttached=%u focusScans=%u\nscreenHooks=%u screenError=%d screenScans=%u screenModeHooks=%u screenModeError=%d screenModeState=%u screenModeClosed=%u\n",unsigned(drive&&drive->view.attached()),unsigned(page),unsigned(page),unsigned(page),unsigned(page),unsigned(drive!=nullptr),unsigned(drive&&drive->view.error()[0]),screen?screen->attached():0U,unsigned(screenMode!=nullptr),unsigned(pixel.ready_context()),featureMask,static_cast<unsigned long long>(now),unsigned(faceLive!=nullptr),unsigned(facePopup&&facePopup->has_source()),unsigned(faceLive&&faceLive->hooks_ready()),faceLive?faceLive->attempts():0U,faceLive?faceLive->found_mask():0U,faceLive?faceLive->stage():0U,faceLive?faceLive->snapshot_stage():0U,faceLive?faceLive->last_rc():0,faceLive?faceLive->failures():0U,focusPopup?focusPopup->last_rc():0,focusPopup?focusPopup->failures():0U,unsigned(focusPopup&&focusPopup->attached()),focusPopup?focusPopup->scans():0U,unsigned(screen&&screen->hooks_ready()),screen?screen->error():0,screen?screen->scans():0U,unsigned(screenMode&&screenMode->hooks_ready()),screenMode?screenMode->error():0,unsigned(screenMode&&screenMode->has_state()),unsigned(screenMode&&screenMode->closed()));if(n>0&&size_t(n)<sizeof(report)){int f=std::snprintf(report+n,sizeof(report)-size_t(n),"flashReady=%u flashPending=%u flashProfile=%u flashDelayUs=%u flashRequested=%u flashSampling=%u flashFault=%u flashError=%u flashErrno=%u flashExposureValid=%u flashElectronic=%u flashDepth=%lld flashTv=%lld\n",unsigned(flashClient.ready),unsigned(flashClient.pending||flashClient.queued),flashClient.state.profile,flashClient.state.delay_us,flashClient.state.requested,flashClient.state.sampling,flashClient.state.fault,flashClient.state.error_code,flashClient.state.error_no,unsigned(observedExposure.valid),unsigned(observedExposure.electronic),(long long)observedExposure.depth,(long long)observedExposure.tv);if(f<=0||size_t(f)>=sizeof(report)-size_t(n))return;n+=f;int z=std::snprintf(report+n,sizeof(report)-size_t(n),"perfCount=%llu gapMaxUs=%llu tickMaxUs=%llu playbackAboutUs=%llu animationUs=%llu metricsUs=%llu pixelUs=%llu driveUs=%llu screenUs=%llu faceUs=%llu focusUs=%llu samplerUs=%llu discoveryUs=%llu driveViewUs=%llu driveProxyUs=%llu driveIntentUs=%llu driveActionUs=%llu guardFlashUs=%llu rootSelectUs=%llu\n",(unsigned long long)timing.count,(unsigned long long)timing.gapMax,(unsigned long long)timing.maximum[0],(unsigned long long)timing.maximum[1],(unsigned long long)timing.maximum[2],(unsigned long long)timing.maximum[3],(unsigned long long)timing.maximum[4],(unsigned long long)timing.maximum[5],(unsigned long long)timing.maximum[6],(unsigned long long)timing.maximum[7],(unsigned long long)timing.maximum[8],(unsigned long long)timing.maximum[9],(unsigned long long)timing.maximum[10],(unsigned long long)(drive?drive->timing.maximum[0]:0),(unsigned long long)(drive?drive->timing.maximum[1]:0),(unsigned long long)(drive?drive->timing.maximum[2]:0),(unsigned long long)(drive?drive->timing.maximum[3]:0),(unsigned long long)timing.maximum[11],(unsigned long long)timing.maximum[12]);if(z>0&&size_t(z)<sizeof(report)-size_t(n)&&x2d_passive_publish(passive,X2D_PASSIVE_UI,report,size_t(n+z),now+2000)){for(auto&v:timing.maximum)v=0;if(drive)for(auto&v:drive->timing.maximum)v=0;timing.count=timing.gapMax=0;}}}
 bool drain(){bool done=pixel.drain();if(!liveviewPreferences.drain())done=false;if(!drainFace())done=false;if(receipt){if(x2d_ui_receipt_release(receipt))receipt=nullptr;else done=false;}if(passive){if(x2d_passive_release(passive))passive=nullptr;else done=false;}return done;}
 static void released(void*opaque){auto*s=static_cast<MenuSession*>(opaque);if(!--s->refs)delete s;}
 static void timer(void*opaque){auto*s=static_cast<MenuSession*>(opaque);if(s->closed){if(!s->drain())s->schedule();return;}if(!s->running)return;
  timespec t{};if(clock_gettime(CLOCK_MONOTONIC,&t)||t.tv_sec<0){s->running=false;x2d_effect_destroy(&s->service);return;}
  int rc=x2d_native_effect_menu_tick_v1(s,1,static_cast<uint64_t>(t.tv_sec)*1000+static_cast<uint64_t>(t.tv_nsec)/1000000);
  if(rc==2||rc==3){s->running=false;x2d_effect_destroy(&s->service);return;}s->schedule();
 }
 bool schedule(){bool faceActive=faceLive&&!faceClosing&&faceLive->hooks_ready()&&faceLive->session().point().snapshot().present&&(faceLive->session().point().snapshot().faceMode||faceLive->session().point().snapshot().eyeMode);++refs;if(!x2d_ui_schedule_image(&image,(animation.active()||faceActive)?16:50,timer,released,this))return true;released(this);running=false;x2d_effect_destroy(&service);return false;}
 // Fresh global window enumeration: no cached foreign QObject is dereferenced.
 void*root(){
  auto windows=reinterpret_cast<x2dui::AbiArray(*)()>(image.base+window_windows)();void*roots[32]{};unsigned count=0;
  if(windows.size>=0&&windows.size<=32&&(windows.data||!windows.size)){
   auto**items=static_cast<void**>(windows.data);for(std::ptrdiff_t n=0;n<windows.size;n++){
    if(!menu.cast(reinterpret_cast<void*>(image.base+window_meta),items[n]))continue;
    auto*root=reinterpret_cast<void*(*)(const void*)>(image.base+window_root)(items[n]);if(root)roots[count++]=root;
   }
  }array_drop(menu,windows);if(count==1)return roots[0];
  void*owned=nullptr,*page=nullptr;unsigned owners=0,pages=0;
  for(unsigned n=0;n<count;n++){if(attachment.belongs(roots[n])){owned=roots[n];++owners;}
   x2dui::MenuTarget target;if(!x2dui::find_exposure_menu(menu,roots[n],target)){page=roots[n];++pages;}}
  return owners==1?owned:pages==1?page:nullptr;
 }

};
extern "C" X2D_UI_INTERNAL_API void*x2d_native_effect_menu_create_v1(){
 if(!x2d_native_features_valid(std::getenv(X2D_NATIVE_UI_FEATURE_ENV))||x2d_native_startup_guard()!=X2D_STARTUP_ALLOW)return nullptr;UiImage image{};if(x2d_ui_runtime_image(&image)||!profile_valid(image))return nullptr;return new(std::nothrow)MenuSession(image);
}
extern "C" X2D_UI_INTERNAL_API int x2d_native_effect_menu_tick_v1(void*handle,int enabled,uint64_t now_ms){
 if(!handle||enabled<0||enabled>1)return 1;auto*s=static_cast<MenuSession*>(handle);
 if(s->closed)return 1;
 s->timing.begin();UiTickTiming::Scope whole(s->timing,0);
 {UiTickTiming::Scope t(s->timing,11);if(x2d_ui_recheck_image(&s->image)||!profile_valid(s->image))return 2;
 if(now_ms<s->now)return 3;s->now=now_ms;x2d_flash_client_poll(&s->flashClient,now_ms);s->observedExposure=x2dui::flash_exposure(factory_proxy_profile(s->image.base));s->attachment.flashService(&s->flashClient,now_ms,s->observedExposure);}
 if(!enabled){x2d_effect_set_enabled(&s->service,0,now_ms);s->animation.close();s->closeFace();s->closeFocus();s->closeScreen();s->closeDrive();s->pixel.suspend(s->root());s->attachment.close(s->root());s->playback.close();s->about.close();s->sampler.close();s->observer.reset();s->lastResult=1;s->receiptTick(nullptr);return 0;}
 void*root=nullptr;{UiTickTiming::Scope t(s->timing,12);root=s->root();}if(!root){x2d_effect_set_enabled(&s->service,0,now_ms);s->animation.close();s->closeFace();s->closeFocus();s->closeScreen();s->closeDrive();s->pixel.suspend(nullptr);s->attachment.close(nullptr);s->playback.close();s->about.close();s->sampler.close();s->observer.reset();s->receiptTick(nullptr);return 4;}
 // Playback status lease is independent of whether exposure settings are open.
 const bool rootChanged=s->capRoot!=root;if(rootChanged){s->capRoot=root;s->effectHooks=s->aboutHooks=false;}{UiTickTiming::Scope t(s->timing,1);s->playback.tick(root,now_ms);s->aboutHooks=s->about.tick(root);}
 double rw=0,rh=0;void*ra[]={&rw,nullptr,nullptr,nullptr};void*rb[]={&rh,nullptr,nullptr,nullptr};s->api.item(root,1,7,ra);s->api.item(root,1,8,rb);x2dui::StockMetricsSnapshot metric;{UiTickTiming::Scope t(s->timing,2);s->animation.tick(root,now_ms,rw,rh);}
 bool metricReady=false;{UiTickTiming::Scope t(s->timing,3);metricReady=s->metrics.read(root,false,metric);}if(metricReady){s->attachment.style(metric.settings.labelFont,metric.settings.valueFont,metric.settings.leftMargin,metric.settings.rightMargin,25.6*metric.scaleY);if(x2d_native_features_advanced(s->featureMask)){{UiTickTiming::Scope t(s->timing,4);s->pixel.tick(root,now_ms,metric,rw,rh);}{UiTickTiming::Scope t(s->timing,5);s->driveTick(root,metric,rw,rh);}}{UiTickTiming::Scope t(s->timing,6);s->screenTick(root,metric,rw,rh);}{UiTickTiming::Scope t(s->timing,7);s->faceTick(root,metric,rw,rh);}{UiTickTiming::Scope t(s->timing,8);s->focusTick(root,metric,rw,rh);}}else{s->animation.close();s->closeFace();s->closeFocus();s->closeScreen();s->closeDrive();s->pixel.suspend(root);}
 s->observeUi();
 {UiTickTiming::Scope t(s->timing,9);timespec wall{};if(!clock_gettime(CLOCK_REALTIME,&wall)&&wall.tv_sec>=0){auto at=uint64_t(wall.tv_sec)*1000+uint64_t(wall.tv_nsec)/1000000;auto*runtime=s->pixel.gui();bool refresh=runtime&&runtime->host().album_pending();if(s->sampler.tick(root,s->playback,refresh,now_ms,at)&&s->passive&&now_ms<=UINT64_MAX-2000)x2d_passive_publish(s->passive,X2D_PASSIVE_PLAYBACK,s->sampler.payload(),s->sampler.size(),now_ms+2000);}
 }
 UiTickTiming::Scope discovery(s->timing,10);bool changed=rootChanged||s->observer.pending();if(!(s->effectHooks=s->observer.refresh(root))){s->attachment.close(root);s->receiptTick(root);return 5;}
 if(!changed){
  x2d_effect_poll(&s->service,now_ms);s->attachment.refresh(s->service.mode,s->service.ready&&!s->service.saving,s->service.ready&&s->service.saving);s->receiptTick(root);return s->lastResult;
 }

 double width=0,height=0;void*wa[]={&width,nullptr,nullptr,nullptr};void*ha[]={&height,nullptr,nullptr,nullptr};s->api.item(root,1,7,wa);s->api.item(root,1,8,ha);
 int rc=s->attachment.tick(root,s->service.mode,s->service.ready&&!s->service.saving,width,height,s->service.ready&&s->service.saving);s->lastResult=rc;
 x2d_effect_poll(&s->service,now_ms);
 s->attachment.refresh(s->service.mode,s->service.ready&&!s->service.saving,s->service.ready&&s->service.saving);s->receiptTick(root);return rc;
}
extern "C" X2D_UI_INTERNAL_API int x2d_native_effect_menu_close_v1(void*handle){
 if(!handle)return 1;auto*s=static_cast<MenuSession*>(handle);if(x2d_ui_recheck_image(&s->image)||!profile_valid(s->image))return 2;
 if(s->closed)return 1;s->closed=true;s->running=false;s->liveviewPreferences.stop();s->animation.close();s->closeFace();s->closeFocus();s->closeScreen();s->closeDrive();delete s->drive;s->drive=nullptr;s->pixel.stop(s->root());x2d_ui_receipt_invalidate(s->receipt);x2d_ui_receipt_stop(s->receipt);x2d_passive_stop(s->passive);s->sampler.close();s->about.close();s->metrics.close();if(!s->drain())s->schedule();x2d_effect_destroy(&s->service);s->attachment.close(s->root());s->playback.close();s->observer.reset();MenuSession::released(s);return 0;
}
// Explicit bootstrap from an already validated GUI-thread integration point.
// No constructor is installed; module remains resident until all Qt slots release.
extern "C" X2D_UI_INTERNAL_API void*x2d_native_effect_menu_start_v1(){
 auto*s=static_cast<MenuSession*>(x2d_native_effect_menu_create_v1());if(!s)return nullptr;s->running=true;
 if(!s->schedule()){x2d_native_effect_menu_close_v1(s);return nullptr;}return s;
}

static void stock_object_destroy(void*object){
 UiImage image{};if(!object||x2d_ui_verified_image(&image))return;
 auto*table=*static_cast<void***>(object);uintptr_t address=reinterpret_cast<uintptr_t>(table);
 if(address<image.base||!ui_range(&image,address-image.base,5*sizeof(void*),PF_R))return;
 uintptr_t fn=reinterpret_cast<uintptr_t>(table[4]);if(fn<image.base||!ui_range(&image,fn-image.base,4,PF_R|PF_X))return;
 reinterpret_cast<void(*)(void*)>(fn)(object);
}
extern "C" X2D_UI_INTERNAL_API void*x2d_native_stock_create_v1(unsigned kind,void*contextObject,void*parentItem){
 if(kind>static_cast<unsigned>(x2dui::StockKind::Touchpad))return nullptr;UiImage image{};if(x2d_ui_runtime_image(&image)||!profile_valid(image))return nullptr;
 auto p=stock_profile(image.base);p.object_destroy=stock_object_destroy;p.valid_vtable=MenuSession::validate_vtable;p.validation_context=&image;
 return x2dui::stock_create(p,static_cast<x2dui::StockKind>(kind),contextObject,parentItem);
}

extern "C" int x2d_ui_register_pre(void(*)(void));
extern "C" int x2d_ui_post_once_v1(void(*)(void*),void(*)(void*),void*);
static unsigned bootstrap_state;static void*bootstrap_handle;
static void bootstrap_create(void*){if(__atomic_load_n(&bootstrap_state,__ATOMIC_ACQUIRE)!=2)return;bootstrap_handle=x2d_native_effect_menu_start_v1();__atomic_store_n(&bootstrap_state,bootstrap_handle?3U:4U,__ATOMIC_RELEASE);}
static void bootstrap_release(void*){unsigned expected=2;__atomic_compare_exchange_n(&bootstrap_state,&expected,4,false,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);}
static void bootstrap_pre(){if(__atomic_load_n(&bootstrap_state,__ATOMIC_ACQUIRE)!=1)return;int rc=x2d_ui_post_once_v1(bootstrap_create,bootstrap_release,nullptr);__atomic_store_n(&bootstrap_state,rc?4U:2U,__ATOMIC_RELEASE);}
extern "C" X2D_UI_INTERNAL_API int x2d_native_ui_register_v1(){
 if(!x2d_native_features_valid(std::getenv(X2D_NATIVE_UI_FEATURE_ENV))){__atomic_store_n(&bootstrap_state,7,__ATOMIC_RELEASE);return 7;}
 int guard=x2d_native_startup_guard();if(guard!=X2D_STARTUP_ALLOW){__atomic_store_n(&bootstrap_state,guard==X2D_STARTUP_DISABLED?6U:8U,__ATOMIC_RELEASE);return guard==X2D_STARTUP_DISABLED?8:9;}
 unsigned expected=0;if(!__atomic_compare_exchange_n(&bootstrap_state,&expected,1,false,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return 6;
 int rc=x2d_ui_register_pre(bootstrap_pre);if(rc)__atomic_store_n(&bootstrap_state,0,__ATOMIC_RELEASE);return rc;
}
extern "C" __attribute__((visibility("default")))unsigned x2d_native_ui_bootstrap_status_v1(){return __atomic_load_n(&bootstrap_state,__ATOMIC_ACQUIRE);}
extern "C" X2D_UI_INTERNAL_API int x2d_native_ui_stop_v1(){
 UiImage image{};if(x2d_ui_runtime_image(&image))return 2;
 if(bootstrap_handle){int rc=x2d_native_effect_menu_close_v1(bootstrap_handle);if(rc)return rc;bootstrap_handle=nullptr;}
 __atomic_store_n(&bootstrap_state,5,__ATOMIC_RELEASE);return 0;
}
