#include "playback_diagnostics.hpp"
#include <QCoreApplication>
#include <QFile>
#include <QJSEngine>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <cstdio>
#include <cstdlib>
using namespace x2dui;
static int assertions;
static void check(bool v){++assertions;if(!v){std::fprintf(stderr,"PLAYBACK_FAIL assertion=%d\n",assertions);std::exit(1);}}
static QString func(const QString&s,const QString&name){auto start=s.indexOf("function "+name+"(");check(start>=0);auto p=s.indexOf('{',start);int depth=1;while(++p<s.size()&&depth){if(s[p]=='{')++depth;else if(s[p]=='}')--depth;}check(!depth);return s.mid(start,p-start)+"\n";}
int main(int argc,char**argv){
 QCoreApplication app(argc,argv);check(argc==2);QFile f(QString::fromLocal8Bit(argv[1]));check(f.open(QIODevice::ReadOnly));auto original=QString::fromUtf8(f.readAll());QJSEngine js;
 auto eval=[&](const QString&s){auto r=js.evaluate(s);if(r.isError())std::fprintf(stderr,"%s\n",qPrintable(r.toString()));check(!r.isError());return r;};
 eval("var enabled=true,samples=0,sampleLimit=1800,sequence=0,events=[],changes=[],lastState='',lastPayload='',refreshPending=false,browse=null,model=null,searchRoot=null,channelDirectory='fixture://';var time=0;Date.now=function(){return time};function XMLHttpRequest(){this.open=function(){};this.send=function(){}};"+func(original,"find")+func(original,"currentPreview")+func(original,"flickable")+func(original,"note")+func(original,"sample"));
 eval(R"JS(
function scene(values,flags,name){
 var current={imageWidth:23326,imageHeight:17498};
 var viewport={visible:flags[1],status:values[9],viewScale:values[10],decodeRect:{width:values[11],height:values[12],x:values[13],y:values[14]},currentViewRect:function(){return{x:values[15],y:values[16]}},regionRequests:values[17],regionReadyMs:values[18],readyTiles:values[19],failedTiles:values[20],fastDecodeMs:values[21],usingFastPath:flags[2],fullFrameHardware:flags[3],activeImage:{paintedWidth:values[22],paintedHeight:values[23]}};
 var full={objectName:'PixelJpegViewportImage',parent:viewport,children:[]};
 var low={objectName:'4kPreview',width:values[6],height:values[7],status:values[8],children:[]};
 var flick={width:values[0],height:values[1],contentWidth:values[2],contentHeight:values[3],contentX:values[4],contentY:values[5],visibleArea:{},children:[]};
 var preview={objectName:'FlickablePreview_root',itemModel:current,visible:true,state:name,children:[flick,low,full]};
 model={currentItemModel:current,browseViewActive:true,zoomed:true,screenBack:true,screenEVF:false,exposing:false};
 browse={objectName:'BrowseView_root',viewModel:model,children:[preview]};searchRoot=browse;
}
)JS");
 PlaybackDiagnostics d;PlaybackSnapshot s;s.browseActive=s.zoomed=s.currentPresent=s.previewPresent=s.screenBack=true;s.modelWidth=23326;s.modelHeight=17498;s.display.flags[PlaybackDisplay::FullPresent]=true;
 const char*reasons[]={"browse-closed","exit-zoom","exposing-changed","screen-evf-changed","screen-back-changed","screen-hdmi-changed","current-item-changed","zoomed-changed","album-refresh"};
 for(int i=0;i<1800;i++){
  uint64_t time=1700000000000ULL+i*500;auto g=js.globalObject();g.setProperty("time",double(time));
  auto values=js.newArray(PlaybackDisplay::MetricCount);for(int k=0;k<PlaybackDisplay::MetricCount;k++){s.display.metrics[k]=double((i/2)%71+k)/8;values.setProperty(k,s.display.metrics[k]);}
  s.display.flags[1]=i&1;s.display.flags[2]=i&2;s.display.flags[3]=i&4;auto flags=js.newArray(4);for(int k=0;k<4;k++)flags.setProperty(k,s.display.flags[k]);
  s.display.previewState=i%2?u"full \"view\"":u"预览\\状态";g.setProperty("values",values);g.setProperty("flags",flags);g.setProperty("stateName",QString::fromUtf16(s.display.previewState.data(),qsizetype(s.display.previewState.size())));
  eval("scene(values,flags,stateName)");g.setProperty("reason",reasons[i%9]);eval("note(reason)");check(d.note(PlaybackEvent(i%9),time));eval("sample()");check(d.sample(s,time));
  auto expected=QJsonDocument::fromJson(g.property("lastPayload").toString().toUtf8());auto actual=QJsonDocument::fromJson(QByteArray(d.payload(),qsizetype(d.size())));check(!expected.isNull()&&!actual.isNull());
  if(expected!=actual){std::fprintf(stderr,"JSON DIFFERENCE iteration=%d\nACTUAL %s\nEXPECTED %s\n",i,actual.toJson(QJsonDocument::Compact).constData(),expected.toJson(QJsonDocument::Compact).constData());}check(expected==actual);
 }
 check(d.samples()==1800);check(!d.note(PlaybackEvent::ExitZoom,0));check(!d.sample(s,0)&&d.size()==0);
 PlaybackDiagnostics paused;paused.set_enabled(false);check(!paused.note(PlaybackEvent::ExitZoom,0));check(!paused.sample(s,0)&&paused.samples()==0);paused.set_enabled(true);check(paused.sample(s,0));check(!paused.note(PlaybackEvent(99),0));
 PlaybackDiagnostics stable;check(stable.sample(s,10));check(stable.sample(s,20));auto json=QJsonDocument::fromJson(QByteArray(stable.payload(),qsizetype(stable.size()))).object();check(json.value("changes").toArray().size()==1);
 s.display.previewState=UiText();for(int i=0;i<256;i++)s.display.previewState+=char16_t(0x2028);for(int i=0;i<8;i++){s.display.metrics[0]=i;check(stable.sample(s,30+i));}check(stable.size()<32768);
 s.display.previewState+=u'x';check(!stable.sample(s,99)&&stable.size()==0);
 std::printf("PLAYBACK_DIAGNOSTICS_PASS assertions=%d original_samples=1800 camera_operations=0\n",assertions);
}
