#pragma once
#include "drive_popup.hpp"
namespace x2dui {
enum class DriveSessionIntentKind {AwaitNormalMode,SetDrive,SetExitDrive,OpenStockSettings,ClosePopup};
struct DriveSessionIntent {uint64_t id=0;DriveSessionIntentKind kind=DriveSessionIntentKind::ClosePopup;DriveChoice choice=DriveChoice::Single;};
enum class DriveSessionResult {Confirmed,Rejected,Unknown};
enum class DriveSessionStatus {Idle,Pending,Complete,Failed,Cancelled};
// GUI-thread state only. Original proxy adapter must confirm each intent from
// current readback; accepting/enqueueing a setter is not confirmation.
class DriveSettingsSession {
 PixelController&controller_;PixelHost&host_;PixelFeatures features_;
 DriveAction actions_[5]{};unsigned count_=0,next_=0;
 DriveSessionIntent pending_{};bool offered_=false,normalChecked_=false;
 uint64_t nextId_=1,deadline_=0;DriveSessionStatus status_=DriveSessionStatus::Idle;
 ExtensionMode page_=ExtensionMode::None;const char16_t*error_=u"";
 bool enqueue(DriveSessionIntentKind,DriveChoice);
 bool allowed(DriveChoice,DrivePermissions)const;
 void fail(const char16_t*);
public:
 DriveSettingsSession(PixelController&c,PixelHost&h,PixelFeatures f={}):controller_(c),host_(h),features_(f){}
 bool dispatch(const DriveAction*,unsigned,uint64_t now,const AlbumContext&,DrivePermissions={});
 void tick(uint64_t now,const AlbumContext&);
 bool take_intent(DriveSessionIntent&,uint64_t now);
 bool complete(uint64_t id,DriveSessionResult,uint64_t now);
 void cancel();
 bool change(const SettingsChange&);
 void leave(SettingsExit);
 // Caller supplies read-only formatted exposure values, never command strings.
 SettingsSnapshot snapshot(const UiText&aperture={},const UiText&shutter={},const UiText&iso={})const;
 ExtensionMode page()const{return page_;}
 DriveSessionStatus status()const{return status_;}
 const char16_t*error()const{return error_;}
 bool pending()const{return status_==DriveSessionStatus::Pending;}
};
}
