#pragma once
#include <cstdint>
namespace x2dui {
struct FocusSnapshot {bool valid=false,selectable=false,allowed[3]{};int mode=255,language=0;};
enum class FocusKey {Previous,Next,Accept,Abort,Other};
enum class FocusAction {None,SetMode,Close};
class FocusModel {
 FocusSnapshot current{};int selected_=-1,pending_=-1;uint64_t deadline_=0;bool failed_=false,closed_=false;
public:
 void observe(const FocusSnapshot&,uint64_t now);
 FocusAction choose(int index,uint64_t now,int&mode);
 void move(int step);
 void abort(){closed_=true;pending_=-1;}
 void reject_pending(){if(pending_>=0){pending_=-1;failed_=true;}}
 bool allowed(int i)const{return !closed_&&current.valid&&current.selectable&&pending_<0&&i>=0&&i<3&&current.allowed[i];}
 int selected()const{return selected_;}int pending()const{return pending_;}
 bool failed()const{return failed_;}bool closed()const{return closed_;}
 int language()const{return current.language;}
};
const char16_t*focus_text(int language,int field);
}
