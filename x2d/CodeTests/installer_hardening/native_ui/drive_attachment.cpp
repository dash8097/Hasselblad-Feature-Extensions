#include "drive_attachment.hpp"
#include <cmath>
namespace x2dui {
static void releaseArray(const MenuProfile&p,AbiArray&a,unsigned size){if(a.owner){auto*r=static_cast<int*>(a.owner);if(__atomic_load_n(r,__ATOMIC_RELAXED)>=0&&__atomic_sub_fetch(r,1,__ATOMIC_ACQ_REL)==0)p.deallocate(a.owner,size,8);}}
static UiText text(const MenuProfile&p,void*o,const char*n){if(!o)return {};auto v=p.property(o,n);auto a=p.string(&v);UiText out;if(a.size<0||a.size>4096||(!a.data&&a.size))out.invalidate();else out=UiText(static_cast<const char16_t*>(a.data),static_cast<size_t>(a.size));releaseArray(p,a,2);p.variant_destroy(&v);return out;}
static bool number(const MenuProfile&p,void*o,const char*n,double&out){if(!o)return false;auto v=p.property(o,n);bool ok=false;out=p.number(&v,&ok);p.variant_destroy(&v);return ok&&std::isfinite(out);}
static void dynamicInteger(const StockProfile&p,void*o,const char*name,int n){alignas(8)unsigned char value[32];p.variant_int(value,n);p.set(o,name,value);p.variant_destroy(value);}
DriveSettingsAttachment::DriveSettingsAttachment(StockProfile s,MenuProfile m,AbiViewProfile*a,SignalProfile g,BindingProfile b,KeyProfile k,KeyMapProfile km,FocusProfile f,DriveAttachmentProfile o,PixelController&c,PixelHost&h,PixelFeatures features):stock(s),menu(m),abi(a),signals(g),binding(b),keys(k),keymap(km),focus(f),original(o),api(abi_view_api(a)),controller(c),host(h),session(c,h,features){permissions.pixel400=features.pixel400;permissions.quarter=features.quarter;permissions.stack=features.rawStack;}
void DriveSettingsAttachment::died(void*p){static_cast<DriveSettingsAttachment*>(p)->lost=true;}
void DriveSettingsAttachment::sceneDied(void*p){auto&s=*static_cast<DriveSettingsAttachment*>(p);s.scene=nullptr;s.lost=true;s.hostIntent={};s.hostOffered=false;}
void DriveSettingsAttachment::sourceDied(void*p){auto&s=*static_cast<DriveSettingsAttachment*>(p);s.sourceControl=nullptr;s.lost=true;s.nextDiscovery=0;}
void DriveSettingsAttachment::loaderDied(void*p){auto&s=*static_cast<DriveSettingsAttachment*>(p);s.sourceLoader=nullptr;s.lost=true;s.nextDiscovery=0;}
void DriveSettingsAttachment::popupLoaded(void*p){auto&s=*static_cast<DriveSettingsAttachment*>(p);if(!s.inTick&&s.scene&&s.sourceControl&&s.sourceLoader)s.tick(s.scene,s.now,s.album,s.driveStyle,s.settingsStyle,s.width,s.height);}
void DriveSettingsAttachment::clearSource(){signal_unwatch(loaderLoaded);signal_unwatch(loaderDeath);loaderLoaded=loaderDeath=nullptr;sourceLoader=nullptr;signal_unwatch(sourceDeath);sourceDeath=nullptr;sourceControl=nullptr;}
bool DriveSettingsAttachment::discoverSource(void*root,uint64_t at){
 if(sourceControl&&sourceLoader)return true;if(!root||(!sourceControl&&at<nextDiscovery))return false;
 nextDiscovery=at>UINT64_MAX-1000?UINT64_MAX:at+1000;
 if(!sourceControl){auto*c=find_named_item(menu,root,u"ControlScreen_focusScope");if(!c)return false;sourceDeath=signal_watch(signals,c,c,"destroyed(QObject*)",sourceDied,this,release);if(!sourceDeath)return false;sourceControl=c;}
 auto*l=find_named_item(menu,sourceControl,u"ControlScreen_popupLoader");if(!l)return false;
 signal_unwatch(loaderLoaded);signal_unwatch(loaderDeath);loaderLoaded=loaderDeath=nullptr;
 loaderDeath=signal_watch(signals,l,l,"destroyed(QObject*)",loaderDied,this,release);
 // Stock 4.2.0 Loader is synchronous. Its existing onLoaded initializes the
 // original model before this later direct connection runs, before paint.
 loaderLoaded=signal_watch(signals,l,l,"loaded()",popupLoaded,this,release);
 if(!loaderDeath||!loaderLoaded){signal_unwatch(loaderLoaded);signal_unwatch(loaderDeath);loaderLoaded=loaderDeath=nullptr;return false;}
 sourceLoader=l;return true;
}
void DriveSettingsAttachment::focusDied(void*p){static_cast<DriveSettingsAttachment*>(p)->returnFocus=nullptr;}
bool DriveSettingsAttachment::named(void*o,const char16_t*name)const{return text(menu,o,"objectName")==name;}
void DriveSettingsAttachment::detach(){
 bool restore=false;if(scene&&returnFocus&&focus.window&&focus.active){auto*w=focus.window(scene);auto*a=w?focus.active(w):nullptr;restore=a&&((settings&&settings->item()&&menu_contains(menu,settings->item(),a))||(popup&&popup->item()&&menu_contains(menu,popup->item(),a)));}
 if(key){key->close();key=nullptr;}if(settings){settings->close();settings=nullptr;}if(popup){popup->close();popup=nullptr;}visibility.close();enabled.close();
 if(restore&&scene&&returnFocus&&menu_contains(menu,scene,returnFocus)&&focus.focus){double visible=0,enabledValue=0;if(number(menu,returnFocus,"visible",visible)&&number(menu,returnFocus,"enabled",enabledValue)&&visible&&enabledValue)focus.focus(returnFocus,7);}
 signal_unwatch(returnWatch);returnWatch=nullptr;returnFocus=nullptr;for(auto&w:deaths){signal_unwatch(w);w=nullptr;}session.cancel();control=model=factoryPopup=grid=swipe=summaryList=nullptr;lost=stockPage=false;
}
void DriveSettingsAttachment::close(){nextDiscovery=0;detach();clearSource();signal_unwatch(sceneDeath);sceneDeath=nullptr;scene=nullptr;hostIntent={};hostOffered=false;ownershipLost=false;}
void*DriveSettingsAttachment::currentModel()const{return scene&&sourceControl?stock_get_object(stock,sourceControl,"viewModel"):nullptr;}
bool DriveSettingsAttachment::dispatch_host_action(const PixelHostAction&a,uint64_t at){
 if(!scene||lost)return false;
 if(a.kind==PixelHostActionKind::ClosePopup){auto*m=currentModel();if(!m)return false;auto state=text(menu,m,"mainState");if(!state.valid())return false;if(state!=u"drive_mode")return true;return stock_set_text(stock,m,"mainState",u"");}
 if(a.kind!=PixelHostActionKind::SetStockSingle||hostIntent.id||session.pending()||controller.state().busy||at>UINT64_MAX-5000||!hostNext)return false;
 hostIntent={hostNext++,DriveSessionIntentKind::SetDrive,DriveChoice::Single};hostDeadline=at+5000;hostOffered=false;hostError=u"";return true;
}
bool DriveSettingsAttachment::complete(uint64_t id,DriveSessionResult result,uint64_t at){if(lost||!scene){session.cancel();hostIntent={};hostOffered=false;return false;}if(id&&id==hostIntent.id){if(!hostOffered)return false;bool timely=at<hostDeadline;hostIntent={};hostOffered=false;if(!timely||result!=DriveSessionResult::Confirmed||controller.state().busy)hostError=u"单张模式切换结果未确认，请查看诊断";return timely;}return session.complete(id,result,at);}
bool DriveSettingsAttachment::locate(void*root){
 auto*c=root==scene?sourceControl:nullptr;if(!c)return false;auto*m=stock_get_object(stock,c,"viewModel");auto*l=find_named_item(menu,c,u"ControlScreen_popupLoader");auto*p=l?stock_get_object(stock,l,"item"):nullptr;
 if(!m||!p||text(menu,m,"mainState")!=u"drive_mode"||!named(p,u"PopoverDriveMode_root"))return false;
 auto*g=find_named_item(menu,p,u"DriveModeGrid_root");auto*s=find_named_item(menu,p,u"PopoverDriveMode_swipe_area");if(!g||!s)return false;
 control=c;model=m;factoryPopup=p;grid=g;swipe=s;return true;
}
bool DriveSettingsAttachment::attach(void*root){
 if(!locate(root))return false;void*watched[]={control,model,factoryPopup,grid,swipe};for(unsigned i=0;i<5;i++){deaths[i]=signal_watch(signals,watched[i],watched[i],"destroyed(QObject*)",died,this,release);if(!deaths[i]){detach();return false;}}
 if(focus.window&&focus.active){auto*w=focus.window(root);auto*old=w?focus.active(w):nullptr;if(old&&menu_contains(menu,root,old)){returnFocus=old;returnWatch=signal_watch(signals,old,control,"destroyed(QObject*)",focusDied,this,release);if(!returnWatch){detach();return false;}}}
 if(!visibility.acquire(stock,binding,signals,factoryPopup,u"visible")||!enabled.acquire(stock,binding,signals,factoryPopup,u"enabled")){detach();return false;}
 popup=DrivePopupView::create(api,stock,signals,control,control,dispatch,summary,this);if(!popup){detach();return false;}
 api.name(api.context,popup->item(),"X2dNativeDriveMode");unsigned count=0,nodes=0;if(!findSummary(factoryPopup,summaryList,count,nodes)||count!=1){detach();return false;}
 deaths[5]=signal_watch(signals,summaryList,summaryList,"destroyed(QObject*)",died,this,release);if(!deaths[5]){detach();return false;}
 // QObject dynamic property is deliberate; setProperty returns false when it
 // creates a dynamic property, so verify its value rather than that return.
 dynamicInteger(stock,popup->item(),"keyOverride",original.halfOverride);double mode=0;if(!number(menu,popup->item(),"keyOverride",mode)||mode!=original.halfOverride){detach();return false;}
 key=NativeKeyFilter::create(keys,signals,popup->item(),pressed,this);if(!key||!visibility.number(0)||!enabled.number(0)){detach();return false;}
 if(focus.focus)focus.focus(popup->item(),0);return !lost;
}
bool DriveSettingsAttachment::dispatch(void*p,const DriveAction*a,unsigned n){auto&v=*static_cast<DriveSettingsAttachment*>(p);double mode=-1;v.permissions.manual=!v.lost&&number(v.menu,v.model,"expMode",mode)&&mode==0;v.controller.set_exposure_mode(v.permissions.manual?0:-1);bool accepted=!v.lost&&!v.hostIntent.id&&v.session.dispatch(a,n,v.now,v.album,v.permissions);if(accepted)v.hostError=u"";return accepted;}
bool DriveSettingsAttachment::changed(void*p,const SettingsChange&v){auto&s=*static_cast<DriveSettingsAttachment*>(p);return !s.lost&&s.session.change(v);}
void DriveSettingsAttachment::left(void*p,SettingsExit e){auto&v=*static_cast<DriveSettingsAttachment*>(p);v.session.leave(e);if(e==SettingsExit::External&&!v.lost&&v.model)stock_set_text(v.stock,v.model,"mainState",u"");}
bool DriveSettingsAttachment::collectRows(void*node,DriveSummary&out,unsigned&visited,unsigned depth){
 if(!node||depth>64||++visited>4096)return false;if(stock_has(stock,node,"text")&&stock_has(stock,node,"valueText")){if(out.count==10)return false;auto label=text(menu,node,"text"),value=text(menu,node,"valueText");if(!label.valid()||!value.valid())return false;out.labels[out.count]=static_cast<UiText&&>(label);out.values[out.count++]=static_cast<UiText&&>(value);return true;}
 auto a=menu.children(node);bool ok=a.size>=0&&a.size<=4096&&(a.data||!a.size);if(ok)for(std::ptrdiff_t i=0;i<a.size&&ok;i++)ok=collectRows(static_cast<void**>(a.data)[i],out,visited,depth+1);releaseArray(menu,a,8);return ok;
}
bool DriveSettingsAttachment::findSummary(void*node,void*&out,unsigned&count,unsigned&visited,unsigned depth){if(!node||depth>64||++visited>4096)return false;
 if(stock_has(stock,node,"settingsList")&&stock_has(stock,node,"description")&&stock_has(stock,node,"headerText")){out=node;++count;return true;}
 auto a=menu.children(node);bool ok=a.size>=0&&a.size<=4096&&(a.data||!a.size);if(ok)for(std::ptrdiff_t i=0;i<a.size&&ok;i++)ok=findSummary(static_cast<void**>(a.data)[i],out,count,visited,depth+1);releaseArray(menu,a,8);return ok;
}
bool DriveSettingsAttachment::summary(void*p,DriveChoice choice,DriveSummary&out){auto&v=*static_cast<DriveSettingsAttachment*>(p);if(v.lost)return false;
 if(DrivePopup::extension(choice)!=ExtensionMode::None){auto snap=v.session.snapshot({}, {}, {});snap.stack=choice==DriveChoice::Stack;MultishotSettings settings;settings.data=static_cast<SettingsSnapshot&&>(snap);out.count=choice==DriveChoice::Stack?5:3;for(int i=0;i<out.count;i++){out.labels[i]=settings.label(i);out.values[i]=settings.value(i);}out.caption=!v.permissions.manual?u"请切换到 M 档后使用":choice==DriveChoice::Pixel?u"选择后应用像素超频模式":u"选择后应用 RAW 堆栈";if(v.error()[0])out.caption=v.error();return out.caption.valid();}
 // The original grid's index only changes its original read-only parameter
 // summary; no setSelected/camera setter is invoked here.
 double index=-1;if(!v.grid||!number(v.menu,v.grid,"currentIndex",index)||(index!=unsigned(choice)&&!stock_set_number(v.stock,v.grid,"currentIndex",unsigned(choice)))||v.lost)return false;
 unsigned visited=0;if(!v.summaryList||!v.collectRows(v.summaryList,out,visited))return false;
 if(choice==DriveChoice::Single)out.caption=u"单张拍摄";else if(choice==DriveChoice::Continuous)out.caption=u"连续拍摄";if(v.error()[0])out.caption=v.error();return out.caption.valid();
}
bool DriveSettingsAttachment::callStockSettings(DriveChoice choice){if(!grid||!swipe||!original.method_index||!original.metacall)return false;const char*name=DrivePopup::settings(choice);if(!name[0])return false;
 const void*table=*static_cast<const void**>(grid);if(!table)return false;const void*fn=*static_cast<const void*const*>(table);if(signals.valid_vtable&&!signals.valid_vtable(signals.validation_context,table,fn))return false;const void*meta=reinterpret_cast<const void*(*)(const void*)>(reinterpret_cast<std::uintptr_t>(fn))(grid);int index=original.method_index(meta,"openDialog(QString)");if(index<0)return false;
 UiText value;for(const char*c=name;*c;c++)value+=char16_t(*c);if(!value.valid())return false;auto string=abi_owned_string(stock,value.data(),value.size());void*args[]={nullptr,&string};original.metacall(grid,0,index,args);abi_release_string(stock,string);if(lost)return false;double showing=0;if(!number(menu,swipe,"show",showing)||showing!=1)return false;
 if(key){key->close();key=nullptr;}visibility.close();enabled.close();stockPage=true;if(popup&&popup->item())api.visible(api.context,popup->item(),false);return true;
}
bool DriveSettingsAttachment::serviceUiIntent(const DriveSessionIntent&i){if(i.kind==DriveSessionIntentKind::ClosePopup){bool ok=!lost&&model&&stock_set_text(stock,model,"mainState",u"");session.complete(i.id,ok?DriveSessionResult::Confirmed:DriveSessionResult::Unknown,now);return true;}
 if(i.kind==DriveSessionIntentKind::OpenStockSettings){bool ok=callStockSettings(i.choice);session.complete(i.id,ok?DriveSessionResult::Confirmed:DriveSessionResult::Unknown,now);return true;}return false;}
bool DriveSettingsAttachment::take_intent(DriveSessionIntent&out,uint64_t at){if(lost||!scene){session.cancel();hostIntent={};hostOffered=false;return false;}if(hostIntent.id){if(at>=hostDeadline||controller.state().busy){hostError=u"单张模式切换已停止，请查看诊断";hostIntent={};hostOffered=false;return false;}if(hostOffered)return false;hostOffered=true;out=hostIntent;return true;}DriveSessionIntent i;if(!session.take_intent(i,at))return false;now=at;if(serviceUiIntent(i))return false;out=i;return true;}
bool DriveSettingsAttachment::showSettings(){
 if(settings)return true;settings=MultishotSettingsView::create(api,stock,signals,abi,control,control,changed,left,this);if(!settings)return false;api.name(api.context,settings->item(),"X2dNativeMultishotSettings");if(key){key->close();key=nullptr;}dynamicInteger(stock,settings->item(),"keyOverride",original.halfOverride);double mode=0;if(!number(menu,settings->item(),"keyOverride",mode)||mode!=original.halfOverride){settings->close();settings=nullptr;return false;}key=NativeKeyFilter::create(keys,signals,settings->item(),pressed,this);if(!key){settings->close();settings=nullptr;return false;}if(focus.focus)focus.focus(settings->item(),0);return true;
}
bool DriveSettingsAttachment::pressed(void*p,const KeyInput&input){auto&v=*static_cast<DriveSettingsAttachment*>(p);if(v.lost||input.repeat||key_unblockable(v.keymap,input))return false;const auto*object=key_instance(v.keymap);if(!object)return false;auto match=[&](int fn){return fn>=0&&v.keymap.is_fn(object,input.key,unsigned(input.modifiers),fn);};auto pair=[&](const int*f){return match(f[0])||match(f[1]);};
 bool back=key_matches(v.keymap,input,KeyFunction::Abort)||key_matches(v.keymap,input,KeyFunction::JstkEscape)||key_matches(v.keymap,input,KeyFunction::Escape);
 // The stock two-column grid uses wheel Up/Down as horizontal navigation.
 // Settings and its numeric selectors are vertical, so both wheel directions
 // must join joystick Up/Down here. Keep the grid mapping below unchanged.
 if(v.settings){if(back)v.settings->key(SettingsKey::Back);else if(pair(v.original.keyUp)||pair(v.original.keyLeft))v.settings->key(SettingsKey::Up);else if(pair(v.original.keyDown)||pair(v.original.keyRight))v.settings->key(SettingsKey::Down);else if(pair(v.original.keyAccept)||match(v.original.keyParameters)||key_matches(v.keymap,input,KeyFunction::Select))v.settings->key(SettingsKey::Accept);else return false;return true;}
 // Apply before the stock Menu handler destroys the popup/model. Consume this
 // press while the asynchronous drive transaction confirms its result.
 if(!v.popup||v.stockPage)return false;if(key_matches(v.keymap,input,KeyFunction::Menu))v.popup->key(DriveKey::Accept);else if(back)v.popup->key(DriveKey::Close);else if(pair(v.original.keyUp))v.popup->key(DriveKey::Up);else if(pair(v.original.keyDown))v.popup->key(DriveKey::Down);else if(pair(v.original.keyLeft))v.popup->key(DriveKey::Left);else if(pair(v.original.keyRight))v.popup->key(DriveKey::Right);else if(pair(v.original.keyAccept))v.popup->key(DriveKey::Accept);else if(match(v.original.keyParameters))v.popup->key(DriveKey::Parameters);else return false;return true;
}
bool DriveSettingsAttachment::updateViews(){
 double actual=0,exp=0,micro=0,evf=0;if(!number(menu,model,"driveMode",actual)||!number(menu,model,"expMode",exp)||!number(menu,model,"lensHasMicroFocusAdjust",micro)||!number(menu,model,"screenEVF",evf))return false;
 DriveChoice choice=DriveChoice::Single;bool found=false;for(unsigned i=0;i<6;i++)if(actual==original.drive[i]){choice=DriveChoice(i);found=true;}if(!found)return false;permissions.fullAuto=exp==original.fullAuto;permissions.manual=exp==0;permissions.microFocusAdjust=micro!=0;touch=evf==0;controller.set_exposure_mode(permissions.manual?0:-1);if(!permissions.manual&&session.page()!=ExtensionMode::None)session.leave(SettingsExit::Back);
 if(session.page()!=ExtensionMode::None){if(!showSettings())return false;if(popup)api.visible(api.context,popup->item(),false);return settings->update(session.snapshot(text(menu,model,"aperture"),text(menu,model,"shutterSpeed"),text(menu,model,"isoAsString")),settingsStyle,width,height,touch);}
 if(settings){if(key){key->close();key=nullptr;}settings->close();settings=nullptr;key=NativeKeyFilter::create(keys,signals,popup->item(),pressed,this);if(!key)return false;if(focus.focus)focus.focus(popup->item(),0);}
 bool ok=popup->update(permissions,choice,host.state().selected,driveStyle,width,height,touch);return ok&&(!error()[0]||popup->reopen_display());
}
bool DriveSettingsAttachment::tick(void*root,uint64_t at,const AlbumContext&a,const DriveStyle&ds,const SettingsStyle&ss,double w,double h){
 if(inTick)return false;inTick=true;struct TickGuard{bool&value;~TickGuard(){value=false;}}guard{inTick};
 now=at;album=a;driveStyle=ds;settingsStyle=ss;width=w;height=h;if(!album.path.valid()){detach();return false;}
 if(!root||root!=scene||lost){if(root!=scene){hostIntent={};hostOffered=false;ownershipLost=false;}detach();clearSource();nextDiscovery=0;signal_unwatch(sceneDeath);sceneDeath=nullptr;scene=root;if(root){sceneDeath=signal_watch(signals,root,root,"destroyed(QObject*)",sceneDied,this,release);if(!sceneDeath){scene=nullptr;return false;}}}if(!root||!discoverSource(root,at))return false;
 if(ownershipLost){auto*m=currentModel();if(m&&text(menu,m,"mainState")==u"drive_mode")return false;ownershipLost=false;return false;}
 if(!popup){auto*m=currentModel();if(!m||text(menu,m,"mainState")!=u"drive_mode"||!attach(root))return false;}
 if(lost||!model||text(menu,model,"mainState")!=u"drive_mode"){detach();return false;}
 if(stockPage){double showing=0;if(!number(menu,swipe,"show",showing)){detach();return false;}if(showing)return true;
  if(!visibility.acquire(stock,binding,signals,factoryPopup,u"visible")||!enabled.acquire(stock,binding,signals,factoryPopup,u"enabled")||!visibility.number(0)||!enabled.number(0)){detach();return false;}stockPage=false;key=NativeKeyFilter::create(keys,signals,popup->item(),pressed,this);if(!key){detach();return false;}if(focus.focus)focus.focus(popup->item(),0);
 }
 if(!visibility.number(0)||!enabled.number(0)){ownershipLost=true;detach();return false;}
 session.tick(now,album);bool ok=updateViews();if(!ok||lost){detach();return false;}return true;
}
}
