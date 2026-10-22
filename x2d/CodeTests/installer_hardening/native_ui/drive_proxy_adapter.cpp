#include "drive_proxy_adapter.hpp"
namespace x2dui {
bool DriveProxyAdapter::begin(const DriveSessionIntent&i,uint64_t now){
 if(active()||!i.id||now>UINT64_MAX-5000)return false;
 if(i.kind!=DriveSessionIntentKind::AwaitNormalMode&&i.kind!=DriveSessionIntentKind::SetDrive&&i.kind!=DriveSessionIntentKind::SetExitDrive)return false;
 if(unsigned(i.choice)>=6)return false;
 pending=i;started=now;deadline=now+5000;session=controller.state().session;statusBefore=controller.status_revision();shutterBefore=controller.shutter_revision();
 if(!session.valid()||controller.state().busy){done(DriveSessionResult::Rejected);return true;}
 if(i.kind==DriveSessionIntentKind::AwaitNormalMode)return true;
 const auto f=i.kind==DriveSessionIntentKind::SetDrive?FactoryField::DriveMode:FactoryField::ExitDriveMode;
 expected=values[unsigned(i.choice)];
 // Official 4.2.0 CameraObjectImpl::setExit_drive_mode (0x18b8d0)
 // ignores values above Continuous. Bracketing/timers keep the prior normal
 // return mode. Verify that valid return mode on a later tick, without sending
 // an unsupported setter or pretending the selected special mode was saved.
 if(i.kind==DriveSessionIntentKind::SetExitDrive&&unsigned(i.choice)>=2){
  if(!factory_read(factory,f,expected)||(expected!=values[0]&&expected!=values[1]))done(DriveSessionResult::Unknown);
  return true;
 }
 if(!factory_write(factory,f,expected))done(DriveSessionResult::Unknown);
 return true;
}
void DriveProxyAdapter::tick(uint64_t now){
 if(!active()||resultReady)return;
 if(now<started||now>=deadline){done(DriveSessionResult::Unknown);return;}
 const auto&s=controller.state();if(!s.session.valid()||s.session!=session||s.busy){done(DriveSessionResult::Unknown);return;}
 if(pending.kind==DriveSessionIntentKind::AwaitNormalMode){
  if(controller.status_revision()<=statusBefore||controller.shutter_revision()<=shutterBefore)return;
  if(s.initialized&&s.ready&&s.unchangedPolls<12&&s.quarterPublished=="DISARMED\n"&&s.lastPublishedArm=="DISARMED\n"&&s.nativeReady&&!s.applied&&!s.preparingMode&&!s.preparedMode&&!s.sending&&!s.shutterPending&&s.nativeArm.empty()&&!s.quarterWanted&&!s.quarterActive&&!s.normalSwitchPending)done(DriveSessionResult::Confirmed);
  return;
 }
 if(now==started)return;
 int64_t actual=0;auto f=pending.kind==DriveSessionIntentKind::SetDrive?FactoryField::DriveMode:FactoryField::ExitDriveMode;
 if(factory_read(factory,f,actual)&&actual==expected)done(DriveSessionResult::Confirmed);
}
bool DriveProxyAdapter::take_result(uint64_t&id,DriveSessionResult&r){if(!active()||!resultReady)return false;id=pending.id;r=result;cancel();return true;}
}
