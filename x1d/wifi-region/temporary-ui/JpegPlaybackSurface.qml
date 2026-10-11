import QtQuick 2.5

Rectangle {
    id:root;objectName:"JpegPlaybackSurface";color:"black"
    property bool loadRequested:false
    property bool jpegComplete:false
    property url jpegSource:""
    property bool cacheEnabled:false
    property bool acceptInteractions:false
    property bool thumbnail:false
    property bool ioReady:true
    property real zoomFactor:1
    property bool nativeZoomPending:false
    property real nativeWidth:0
    property real nativeHeight:0
    property rect tileRegion:Qt.rect(0,0,1,1)
    property url tileSource:""
    readonly property real fitScale:nativeWidth>0 && nativeHeight>0?Math.min(width/nativeWidth,height/nativeHeight):1
    readonly property real photoWidth:nativeWidth>0?nativeWidth*fitScale*zoomFactor:viewport.contentWidth
    readonly property real photoHeight:nativeHeight>0?nativeHeight*fitScale*zoomFactor:viewport.contentHeight
    readonly property bool fullResolution:zoomFactor>1 || nativeZoomPending
    property alias imageStatus:preview.status
    property alias displayedSource:preview.source
    readonly property bool acceptedSource:/^file:\/\/.*\.jpe?g$/i.test(String(jpegSource)) || String(jpegSource).indexOf("image://hbljpeg/")===0
    readonly property bool displayReady:loadRequested && jpegComplete && acceptedSource && preview.status===Image.Ready
    property url retainedSource:""
    signal tapped()
    signal doubleTapped()
    function syncSource(){
        if(!loadRequested || !jpegComplete || !acceptedSource){retainedSource="";zoomFactor=1;nativeZoomPending=false}
        else if(ioReady)retainedSource=jpegSource
    }
    onLoadRequestedChanged:syncSource()
    onJpegCompleteChanged:syncSource()
    onJpegSourceChanged:syncSource()
    onAcceptedSourceChanged:syncSource()
    onIoReadyChanged:syncSource()
    Component.onCompleted:syncSource()
    function reload(){retainedSource="";syncSource()}
    function toggleNativeZoom(){
        if(fullResolution){nativeZoomPending=false;zoomFactor=1}
        else {zoomFactor=nativeWidth>0 && nativeHeight>0?Math.max(1,1/fitScale):2}
        centerTimer.restart()
    }
    onZoomFactorChanged:{if(zoomFactor===1){viewport.contentX=0;viewport.contentY=0;tileSource=""}else tileTimer.restart()}
    function refreshTile(){
        if(!fullResolution || !retainedSource || !nativeWidth || !nativeHeight){tileSource="";return}
        var left=(viewport.contentWidth-photoWidth)/2,top=(viewport.contentHeight-photoHeight)/2
        var x=Math.max(0,(viewport.contentX-left-width*0.25)/photoWidth),y=Math.max(0,(viewport.contentY-top-height*0.25)/photoHeight)
        var right=Math.min(1,(viewport.contentX-left+width*1.25)/photoWidth),bottom=Math.min(1,(viewport.contentY-top+height*1.25)/photoHeight)
        if(right<=x || bottom<=y)return
        var ix=Math.floor(x*1000000),iy=Math.floor(y*1000000),iw=Math.min(1000000-ix,Math.ceil((right-x)*1000000)),ih=Math.min(1000000-iy,Math.ceil((bottom-y)*1000000))
        tileRegion=Qt.rect(ix/1000000,iy/1000000,iw/1000000,ih/1000000)
        var source=String(retainedSource)
        if(source.indexOf("image://hbljpeg/")===0)tileSource="image://hbljpeg/region/"+[ix,iy,iw,ih].join('/')+"/"+source.substring(16)
    }
    Timer {id:tileTimer;interval:80;onTriggered:root.refreshTile()}
    PinchArea {
        anchors.fill:parent;enabled:root.acceptInteractions
        property real startZoom:1
        property real anchorX:0.5
        property real anchorY:0.5
        onPinchStarted:{
            viewport.cancelFlick();root.nativeZoomPending=false
            startZoom=root.zoomFactor
            anchorX=(viewport.contentX+pinch.center.x)/viewport.contentWidth
            anchorY=(viewport.contentY+pinch.center.y)/viewport.contentHeight
        }
        onPinchUpdated:{
            root.zoomFactor=Math.max(1,Math.min(24,startZoom*pinch.scale))
            viewport.contentX=Math.max(0,Math.min(viewport.contentWidth-viewport.width,anchorX*viewport.contentWidth-pinch.center.x))
            viewport.contentY=Math.max(0,Math.min(viewport.contentHeight-viewport.height,anchorY*viewport.contentHeight-pinch.center.y))
        }
        Flickable {
            id:viewport;objectName:"JpegPanViewport";anchors.fill:parent;clip:true
            interactive:root.fullResolution
            boundsBehavior:Flickable.StopAtBounds
            contentWidth:width*root.zoomFactor;contentHeight:height*root.zoomFactor
            onContentXChanged:if(root.fullResolution)tileTimer.restart()
            onContentYChanged:if(root.fullResolution)tileTimer.restart()
            Image {
                id:preview;objectName:"JpegPlaybackPixels"
                width:viewport.contentWidth;height:viewport.contentHeight
                asynchronous:true;cache:root.cacheEnabled;fillMode:Image.PreserveAspectFit
                source:root.retainedSource
                sourceSize:Qt.size(Math.max(1,Math.ceil(root.width)),Math.max(1,Math.ceil(root.height)))
                visible:root.displayReady
            }
            Image {
                id:full;objectName:"JpegFullPixels"
                x:(viewport.contentWidth-root.photoWidth)/2+root.tileRegion.x*root.photoWidth
                y:(viewport.contentHeight-root.photoHeight)/2+root.tileRegion.y*root.photoHeight
                width:root.tileRegion.width*root.photoWidth;height:root.tileRegion.height*root.photoHeight
                asynchronous:true;cache:false;fillMode:Image.PreserveAspectFit
                source:root.fullResolution?root.tileSource:""
                visible:root.fullResolution && status===Image.Ready
            }
            MouseArea {
                width:viewport.contentWidth;height:viewport.contentHeight
                enabled:root.acceptInteractions
                onClicked:root.tapped()
                onDoubleClicked:root.doubleTapped()
            }
        }
    }
    Timer {id:centerTimer;interval:0;onTriggered:{viewport.contentX=(viewport.contentWidth-viewport.width)/2;viewport.contentY=(viewport.contentHeight-viewport.height)/2}}
}
