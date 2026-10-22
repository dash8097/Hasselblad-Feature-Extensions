#include "drive_popup.hpp"
#include <cstdio>
#include <cstdlib>
using namespace x2dui;
static int checks;static void ck(bool v){++checks;if(!v){std::fprintf(stderr,"DRIVE_FAIL %d\n",checks);std::exit(1);}}
int main(){
 for(unsigned mask=0;mask<32;mask++)for(int actual=0;actual<6;actual++)for(int selected=0;selected<3;selected++){
  DrivePermissions p{bool(mask&1),bool(mask&2),bool(mask&4),bool(mask&8),bool(mask&16)};DrivePopup d;d.configure(p,DriveChoice(actual),ExtensionMode(selected));ck(d.count()==6+int(p.pixel400||p.quarter)+int(p.stack));
  for(int i=0;i<d.count();i++){auto c=d.choice(i);bool yes=!((p.fullAuto&&(c==DriveChoice::Interval||c==DriveChoice::ExposureBracketing||c==DriveChoice::FocusBracketing))||(c==DriveChoice::FocusBracketing&&!p.microFocusAdjust));ck(d.selectable(i)==yes);}
 }
 for(int i=0;i<8;i++){DrivePopup d;d.configure({false,true,true,true,true},DriveChoice::Single,ExtensionMode::None);ck(d.select_index(i));ck(d.select(true));DriveAction a;int count=0,applies=0;while(d.take(a)){++count;if(a.kind==DriveActionKind::ApplyExtension)++applies;}ck(count==(i>=6?3:4));ck(applies==(i>=6?1:0));ck(d.close());ck(!d.take(a));ck(!d.select(true));}
 {DrivePopup d;ck(d.press(6));DriveAction a;ck(d.take(a)&&a.kind==DriveActionKind::SelectExtension);ck(!d.take(a));ck(!d.closed());ck(d.close());int applies=0;while(d.take(a))if(a.kind==DriveActionKind::ApplyExtension)++applies;ck(applies==1);ck(d.close());ck(!d.take(a));}
 {DrivePopup d;for(int i=0;i<16;i++){ck(d.select_index(6));ck(d.enter());}ck(!d.close());ck(!d.closed()&&!d.committed());DriveAction a;ck(d.take(a));ck(!d.close());ck(d.take(a));ck(!d.close());ck(d.take(a));ck(d.close());ck(d.closed()&&d.committed());}
 {DrivePopup d;d.configure({true,false,true,true,true},DriveChoice::Single,ExtensionMode::None);d.move(0,1);ck(d.index()==2);d.move(0,1);ck(d.index()==6);d.move(0,1);ck(d.index()==6);d.move(1,0);ck(d.index()==7);d.move(-1,0);ck(d.index()==6);}
 for(int n=0;n<1000;n++){DrivePopup d;DriveAction a;ck(d.press(6));ck(d.enter());ck(d.close());int applies=0,opens=0;while(d.take(a)){if(a.kind==DriveActionKind::ApplyExtension)++applies;if(a.kind==DriveActionKind::OpenExtensionSettings)++opens;}ck(applies==1&&opens==1);}
 std::printf("DRIVE_POPUP_PASS assertions=%d camera_operations=0\n",checks);
}
