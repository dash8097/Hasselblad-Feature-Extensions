#pragma once
namespace x2dui {
// 对应 p65，只决定 GUI 接收状态；不触发对焦、快门或取消相机任务。
struct QuarterKeyPolicy {
 bool active=false,armed=false,shutterHeld=false,halfHeld=false;
 bool intercepting()const{return active||armed||shutterHeld||halfHeld;}
 bool overrideKeys()const{return active;}
 bool pressed(bool keyE,bool mappedHalfPress,bool unblockable){
  if(keyE){shutterHeld=true;return true;}
  if(armed&&mappedHalfPress){halfHeld=true;return true;}
  return active&&!unblockable;
 }
 bool released(bool keyE,bool mappedHalfPress,bool unblockable){
  if(keyE){shutterHeld=false;return true;}
  if(halfHeld&&mappedHalfPress){halfHeld=false;return true;}
  return active&&!unblockable;
 }
 // 调用方持有原QObject弱引用；目标有效且焦点未被别的页面夺走才可恢复。
 bool mayRestoreFocus(bool targetAlive,bool targetVisible,bool targetEnabled,bool sameWindow,bool focusUnclaimed)const{
  return !intercepting()&&targetAlive&&targetVisible&&targetEnabled&&sameWindow&&focusUnclaimed;
 }
};
}
