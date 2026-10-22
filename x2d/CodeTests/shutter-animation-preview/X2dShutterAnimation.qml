import QtQuick

// 仅观察现有 GUI 状态；声音请求只发往本机临时预载播放器。
Item {
    id: root
    objectName: "X2dShutterAnimation"
    anchors.fill: parent
    property var stateSource: null
    readonly property string hostState: stateSource ? stateSource.mainState : ""
    property bool showing: false
    property real progress: 0
    property int playCount: 0
    property bool warmed: false
    property bool priming: true
    property bool audioReady: false
    property int audioChecks: 0
    property alias preferences: effectPreferences
    property bool networkEnabled: true
    property double startedAt: 0
    visible: true
    opacity: showing ? 1 : (priming ? 0.001 : 0)
    enabled: false
    z: 1000
    function clamp(x) { return Math.max(0, Math.min(1, x)) }
    function smooth(x) { x=clamp(x); return x*x*(3-2*x) }
    // 本地候选四段各 200 ms，共 800 ms；比例与共享仓库 400 ms 版相同。
    readonly property real closure: clamp(progress/(100/400))*(1-clamp((progress-200/400)/(100/400)))
    // 八边形开口斜边到达矩形四角；以对角线范围展开，不留遮挡。
    readonly property real openRadius: 400+300*Math.tan(Math.PI/8)
    readonly property real apertureRadius: progress < 100/400 ? 250*(1-clamp(progress/(100/400)))
                                           : progress < 200/400 ? 0 : openRadius*clamp((progress-200/400)/(100/400))
    readonly property real flight: clamp((progress-85/400)/(315/400))
    function soundRequest(action) {
        if (!networkEnabled) return
        var request=new XMLHttpRequest()
        request.onreadystatechange=function() {
            if(request.readyState !== XMLHttpRequest.DONE) return
            if(action === "ready") {
                audioReady=request.status === 200
                console.info("X2D_SHUTTER_AUDIO_READY", audioReady)
            }
        }
        request.open("GET", "http://127.0.0.1:38491/"+action)
        request.send()
    }
    function begin() {
        if (!preferences.ready || preferences.mode === 0) return
        motion.stop()
        progress=0
        showing=true
        startedAt=Date.now()
        playCount+=1
        motion.start()
        if(audioReady && preferences.mode === 2) soundRequest("play")
        watchdog.restart()
        console.info("X2D_SHUTTER_BEGIN", playCount)
    }
    function finish(reason) {
        if (!showing) return
        motion.stop()
        watchdog.stop()
        showing=false
        console.info("X2D_SHUTTER_END", reason, Date.now()-startedAt)
    }
    onHostStateChanged: {
        if (hostState === "exposing") begin()
        else finish("stock_state_restored")
    }
    onStateSourceChanged: { if (!stateSource) finish("source_lost") }
    NumberAnimation {
        id: motion
        target: root
        property: "progress"
        from: 0
        to: 1
        duration: 800
        easing.type: Easing.Linear
        // 演示结束不控制相机；只保留纯黑底至原厂退出 exposing。
    }
    Timer { id: watchdog; interval: 2000; onTriggered: root.finish("overlay_timeout") }
    Timer { running: true; interval: 80; onTriggered: root.priming=false }
    // 启动时设备和 GUI 可并行加载；有界等待，只检查就绪，不触发音频。
    Timer {
        interval: 300; repeat: true
        running: root.networkEnabled && !root.audioReady && root.audioChecks < 34
        onTriggered: { root.audioChecks+=1; root.soundRequest("ready") }
    }
    X2dEffectPreferences { id: effectPreferences; networkEnabled: root.networkEnabled }
    Connections {
        target: preferences
        function onModeChanged() { if (preferences.mode === 0) root.finish("effects_disabled") }
    }
    Rectangle { anchors.fill: parent; color: "black" }
    Item {
        id: design
        width: 800
        height: 600
        anchors.centerIn: parent
        scale: Math.min(root.width/800, root.height/600)
        Row {
            anchors.centerIn: parent
            opacity: 1
            Text { text: "Ciallo～(∠・ω< )⌒"; color: "white"; font.pixelSize: 40 }
            Text { text: "☆"; color: "white"; font.pixelSize: 40; opacity: root.progress < 85/400 || root.progress >= 1 ? 1 : 0 }
        }
        Canvas {
            id: leaves
            anchors.fill: parent
            opacity: 1
            renderTarget: Canvas.Image
            onAvailableChanged: if (available) requestPaint()
            onPaint: {
                var ctx=getContext("2d")
                ctx.clearRect(0,0,800,600)
                var radius=root.apertureRadius, twist=.60*root.closure
                ctx.fillStyle="#131313"
                ctx.strokeStyle="#626262"
                ctx.lineWidth=1.2
                for (var i=0;i<8;i++) {
                    var a=i*Math.PI/4+twist,b=a+Math.PI/4
                    ctx.beginPath()
                    ctx.moveTo(400+radius*Math.cos(a),300+radius*Math.sin(a))
                    ctx.lineTo(400+radius*Math.cos(b),300+radius*Math.sin(b))
                    ctx.quadraticCurveTo(400+720*Math.cos(b+.25),300+720*Math.sin(b+.25),400+1500*Math.cos(b+.8),300+1500*Math.sin(b+.8))
                    ctx.lineTo(400+1500*Math.cos(a+.8),300+1500*Math.sin(a+.8))
                    ctx.quadraticCurveTo(400+720*Math.cos(a+.25),300+720*Math.sin(a+.25),400+radius*Math.cos(a),300+radius*Math.sin(a))
                    ctx.closePath()
                    ctx.fill()
                    ctx.stroke()
                }
                if (!root.warmed) {
                    root.warmed=true
                    console.info("X2D_SHUTTER_PRIMED")
                }
            }
            Connections { target: root; function onClosureChanged() { leaves.requestPaint() } function onApertureRadiusChanged() { leaves.requestPaint() } }
        }
        Text {
            text: "☆"
            color: "white"
            font.pixelSize: 100
            x: 400+300*root.flight-width/2
            y: 300-800*root.flight*(1-root.flight)+60*root.flight-height/2
            scale: root.smooth(root.flight/.25)*(1+.20*Math.sin(root.flight*Math.PI))
            rotation: -20+40*root.flight
            opacity: root.smooth(root.flight/.1)*(1-root.smooth((root.progress-.92)/.08))
        }
    }
    Component.onCompleted: {
        console.info("X2D_SHUTTER_READY", width, height)
        soundRequest("ready")
    }
}
