#include "animation_attachment.hpp"
#include "protocol_text.hpp"
namespace x2dui {
static void releaseArray(const MenuProfile&p,AbiArray&a,unsigned size){if(a.owner&&__atomic_load_n(static_cast<int*>(a.owner),__ATOMIC_RELAXED)>=0&&__atomic_sub_fetch(static_cast<int*>(a.owner),1,__ATOMIC_ACQ_REL)==0)p.deallocate(a.owner,size,8);}
static UiText readText(const MenuProfile&p,void*o,const char*n){if(!o)return {};auto v=p.property(o,n);auto a=p.string(&v);UiText s;if(a.size<0||a.size>4096||(!a.data&&a.size))s.invalidate();else s=UiText(static_cast<const char16_t*>(a.data),size_t(a.size));releaseArray(p,a,2);p.variant_destroy(&v);return s;}
static UiText objectName(const MenuProfile&p,void*o){auto a=p.name(o);UiText s;if(a.size<0||a.size>4096||(!a.data&&a.size))s.invalidate();else s=UiText(static_cast<const char16_t*>(a.data),size_t(a.size));releaseArray(p,a,2);return s;}
void AnimationAttachment::rootDestroyed(void*p){auto&s=*static_cast<AnimationAttachment*>(p);s.root=s.drawer=nullptr;s.lost=true;s.core.stop();s.stateDirty=true;}
void AnimationAttachment::sourceDestroyed(void*p){auto&s=*static_cast<AnimationAttachment*>(p);s.drawer=nullptr;s.core.stop();s.stateDirty=true;}
void AnimationAttachment::stateChanged(void*p){auto&s=*static_cast<AnimationAttachment*>(p);s.stateDirty=true;if(!s.ticking&&!s.closing&&!s.lost&&s.clock.now){TickScope guard(s);s.observe(s.clock.now(s.clock.context));}}
void AnimationAttachment::finishTick(){ticking=false;if(closePending)close();}
bool AnimationAttachment::discover(void*o,void*&found,unsigned&hits,unsigned&visited,unsigned depth){
 if(!o||depth>64||++visited>4096)return false;auto n=objectName(menu,o);if(!n.valid())return false;if(n.size()>=9&&!std::memcmp(n.data(),u"X2dNative",18))return true;
 if(n==u"ControlDrawer_root"){found=o;++hits;}
 auto a=menu.children(o);bool ok=a.size>=0&&a.size<=4096&&(a.data||!a.size);if(ok)for(std::ptrdiff_t i=0;i<a.size&&ok;i++)ok=discover(static_cast<void**>(a.data)[i],found,hits,visited,depth+1);releaseArray(menu,a,8);return ok;
}
bool AnimationAttachment::bindSource(void*fresh){
 if(fresh==drawer)return true;signal_unwatch(stateChange);signal_unwatch(drawerDeath);stateChange=drawerDeath=nullptr;drawer=fresh;core=ShutterAnimationCore{};stateDirty=true;
 if(!drawer)return true;
 drawerDeath=signal_watch(signals,drawer,root,"destroyed(QObject*)",sourceDestroyed,this,released);
 stateChange=signal_watch(signals,drawer,root,"mainStateChanged()",stateChanged,this,released);
 if(!drawerDeath||!stateChange){signal_unwatch(stateChange);signal_unwatch(drawerDeath);stateChange=drawerDeath=nullptr;drawer=nullptr;return false;}return true;
}
bool AnimationAttachment::observe(uint64_t now){
 if(!effect||lost||!root)return false;auto state=readText(menu,drawer,"mainState");if(!state.valid())return false;
 bool play=core.update(drawer!=nullptr,state==u"exposing",effect->ready,effect->mode,sound.ready,true,now);stateDirty=false;
 if(play)x2d_sound_play(&sound,now);return true;
}
bool AnimationAttachment::tick(void*freshRoot,uint64_t now,double width,double height){
 if(ticking||closing||!clock.now||!effect)return false;
 if(freshRoot!=root||lost){close();root=freshRoot;lost=false;}
 if(!root)return false;TickScope guard(*this);const auto epoch=generation;
 if(!rootDeath){rootDeath=signal_watch(signals,root,root,"destroyed(QObject*)",rootDestroyed,this,released);if(!rootDeath)return false;}
 x2d_effect_set_enabled(effect,1,now);x2d_effect_poll(effect,now);
 x2d_sound_set_enabled(&sound,effect->ready&&effect->mode==2,now);x2d_sound_poll(&sound,now);
 bool search=tree.pending()||!drawer;if(!tree.refresh(root)||lost||generation!=epoch)return false;
 if(search){void*found=nullptr;unsigned hits=0,visited=0;if(!discover(root,found,hits,visited,0)||hits>1){bindSource(nullptr);if(view)view->hide();return false;}if(!bindSource(found))return false;}
 if(!observe(now)||lost||generation!=epoch){if(view)view->hide();return false;}
 if(!effect->ready||effect->mode<1){if(view){view->close();view=nullptr;}return true;}
 if(!view){view=NativeAnimationView::create(api,stock,menu,image,signals,root);if(!view)return false;}
 bool ok=view->update(core.frame(now),width,height);if(!ok){view->close();view=nullptr;}return ok&&!lost&&generation==epoch;
}
void AnimationAttachment::close(){
 ++generation;if(closing)return;if(ticking){closePending=true;return;}closing=true;closePending=false;
 signal_unwatch(stateChange);signal_unwatch(drawerDeath);signal_unwatch(rootDeath);stateChange=drawerDeath=rootDeath=nullptr;
 tree.reset();if(view){view->close();view=nullptr;}core=ShutterAnimationCore{};root=drawer=nullptr;lost=false;stateDirty=true;
 x2d_sound_destroy(&sound);x2d_sound_init(&sound);if(effect)x2d_effect_set_enabled(effect,0,clock.now?clock.now(clock.context):0);closing=false;
}
}
