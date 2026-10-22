#pragma once
#include "pixel_host_state.hpp"
#include "multishot_settings.hpp"
namespace x2dui {
enum class DriveChoice {Single,Continuous,SelfTimer,Interval,ExposureBracketing,FocusBracketing,Pixel,Stack};
enum class DriveActionKind {SelectExtension,ApplyExtension,SetDrive,SetExitDrive,OpenStockSettings,OpenExtensionSettings,Close};
struct DriveAction {DriveActionKind kind=DriveActionKind::Close;DriveChoice choice=DriveChoice::Single;};
struct DrivePermissions {bool fullAuto=false,microFocusAdjust=false,pixel400=true,quarter=true,stack=true,manual=true;};
// Only typed intents. The integration owns fixed original enum values and proxy calls.
class DrivePopup {
 DrivePermissions permissions_;DriveChoice choices_[8]{};int count_=0,index_=0;bool committed_=false,closed_=false;
 DriveAction actions_[16]{};unsigned start_=0,countActions_=0;
 bool enqueue(const DriveAction*,unsigned);
public:
 DrivePopup(){configure({},DriveChoice::Single,ExtensionMode::None);}
 void configure(DrivePermissions,DriveChoice actual,ExtensionMode selected);
 int count()const{return count_;}int index()const{return index_;}
 DriveChoice choice(int i)const{return i>=0&&i<count_?choices_[i]:DriveChoice::Single;}
 bool selectable(int)const;bool can_settings(int)const;
 bool select_index(int);void move(int columns,int rows);
 bool select(bool close);bool press(int);bool enter();bool close();
 // Display-only recovery: preserve selection; discard consumed intents and make Close passive.
 void reopen_after_failure(){closed_=false;committed_=true;start_=countActions_=0;}
 bool take(DriveAction&);bool closed()const{return closed_;}bool committed()const{return committed_;}
 static ExtensionMode extension(DriveChoice);
 static const char*settings(DriveChoice);
 static const char16_t*label(DriveChoice);
 static const char16_t*icon(DriveChoice,bool disabled=false);
};
}
