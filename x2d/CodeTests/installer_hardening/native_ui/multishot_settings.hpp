#pragma once
#include "protocol_text.hpp"
namespace x2dui {
enum class SettingsOption {None,Resolution,PixelDelay,Keep,Method,Noise,TotalSeconds,StackDelay,TargetIso,Hours,Minutes,Seconds};
struct SettingsChange {SettingsOption option=SettingsOption::None;int value=0,alsoTargetIso=0;};
struct SettingsSnapshot {
 bool controller=false,busy=false,pixel400=true,quarter=true,stack=false,noise=false,keep=false;
 int resolution=6,pixelDelay=2,method=2,totalSeconds=60,stackDelay=2,targetIso=100;
 int frames=-1,plannedFrames=0;double gapSeconds=-1,outputIso=0,outputSeconds=0;
 bool applied=false,controllerIso=false;int controllerTargetIso=100,controllerPlannedFrames=0;
 UiText aperture,shutter,iso,outputShutter;
};
struct SettingsChoices {
 SettingsOption option=SettingsOption::None;int count=0,index=-1,values[64]{};UiText labels[64];
 bool valid=true;
};
enum class SettingsExit {None,Back,External};
class MultishotSettings {
public:
 SettingsSnapshot data;
 bool editingTime=false,popup=false;int currentRow=0;
 int source_iso()const;
 int planned_frames()const;
 int row_count()const;
 bool editable()const{return data.controller&&!data.busy;}
 bool row_editable(int)const;
 UiText title()const;
 UiText label(int)const;
 UiText value(int)const;
 bool open_row(int);
 void close_popup(){popup=false;}
 SettingsExit leave(bool external);
 void move(int direction);
 SettingsChoices choices()const;
 SettingsChange choose(SettingsOption,double value);
 static UiText duration(int);
};
}
