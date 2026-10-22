#pragma once
#include <cstddef>
namespace x2dui {
enum class ProcessingStage {Idle,Prepare,Delay,Capture,Merge,Render,Jpeg,JpegJoin,Package,Convert,Raw,Publish,Restore,Cleanup,Complete,Failed,Unknown};
struct ProcessingState {
 bool present=false,busy=false,ready=false,preparing=false,stack=false,iso=false,joinComplete=false;
 ProcessingStage stage=ProcessingStage::Idle;
 int resolution=6,captureFrames=0,stackRemaining=0,stackFrames=0,plannedFrames=0,renderTiles=0,jpegTiles=0;
 double gapMs=0,outputIso=0,joinRows=0,joinTotal=0,workDone=0,workTotal=0;
 const char16_t*applicationError=u"",*lastError=u"",*shutter=u"",*countdown=u"";
};
struct ProcessingPresentation {
 char16_t title[64]{},status[160]{},quantity[80]{},detail[1024]{},button[16]{},countdown[80]{};
 bool counting=false,showButtons=false,finishAllowed=false,cancelAllowed=false,infinite=false;
 double progress=0;
};
ProcessingPresentation processing_presentation(const ProcessingState&);
}
