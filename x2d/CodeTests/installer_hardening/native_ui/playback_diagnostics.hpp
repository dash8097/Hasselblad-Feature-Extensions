#pragma once
#include "protocol_text.hpp"
namespace x2dui {
struct PlaybackDisplay {
 enum Metric {ScreenWidth,ScreenHeight,ContentWidth,ContentHeight,ContentX,ContentY,PreviewWidth,PreviewHeight,PreviewStatus,FullStatus,FullScale,DecodeWidth,DecodeHeight,DecodeX,DecodeY,EffectiveX,EffectiveY,RegionRequests,RegionReadyMs,ReadyTiles,FailedTiles,FastDecodeMs,PaintedWidth,PaintedHeight,MetricCount};
 enum Flag {FullPresent,FullVisible,FastPath,FullFrameHardware,FlagCount};
 UiText previewState;double metrics[MetricCount]{};bool flags[FlagCount]{};
 PlaybackDisplay(){metrics[PreviewStatus]=metrics[FullStatus]=-1;}
};
struct PlaybackSnapshot {
 bool browseActive=false,zoomed=false,refreshPending=false,currentPresent=false,previewPresent=false;
 double modelWidth=0,modelHeight=0;bool screenBack=false,screenEVF=false,exposing=false;
 PlaybackDisplay display;
};
enum class PlaybackEvent {BrowseClosed,ExitZoom,ExposingChanged,ScreenEvfChanged,ScreenBackChanged,ScreenHdmiChanged,CurrentItemChanged,ZoomedChanged,AlbumRefresh};
// 仅数值和固定事件；无照片身份、路径、元数据、像素或调试脚本入口。
class PlaybackDiagnostics {
 struct Change {uint64_t at=0;PlaybackDisplay display;};
 struct Event {uint64_t at=0;PlaybackEvent reason=PlaybackEvent::BrowseClosed;};
 Change changes_[8];Event events_[16];unsigned changeStart_=0,changeCount_=0,eventStart_=0,eventCount_=0;
 PlaybackDisplay last_;bool hasLast_=false,enabled_=true;unsigned samples_=0;
 char payload_[32768]{};size_t length_=0;
public:
 void set_enabled(bool value){enabled_=value;}
 unsigned samples()const{return samples_;}
 bool note(PlaybackEvent,uint64_t wallMilliseconds);
 // 由原生500ms GUI定时器调用；返回false时禁止发布旧/截断内容。
 bool sample(const PlaybackSnapshot&,uint64_t wallMilliseconds);
 const char*payload()const{return payload_;}size_t size()const{return length_;}
};
}
