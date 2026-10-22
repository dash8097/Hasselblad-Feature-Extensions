import QtQuick

Item {
    id: root
    visible: false
    property string channelDirectory: "file:///tmp/x2d-pixel-overclock/"
    property string session: ""
    property int acknowledgement: 0
    property int completedAcknowledgement: -1
    property int initialDelay: 2
    readonly property int pixelDelay: pixelResolution === 25 ? 0 : initialDelay
    property bool keepMode: false
    property bool stackMode: false
    property int stackMethod: 2
    property bool stackNoise: false
    property int stackSeconds: 60
    property int stackInitialDelay: 2
    property int stackTargetIso: 100
    property int stackSourceIso: 0
    property int stackPlannedFrames: 0
    property real stackShutterNumerator: 0
    property real stackShutterDenominator: 0
    readonly property bool stackIsoMode: stackMode && stackMethod === 3
    readonly property real stackOutputIso: stackIsoMode && stackFrameCount>0 ? stackSourceIso/stackFrameCount : 0
    readonly property real stackOutputSeconds: stackIsoMode && stackFrameCount>0 && stackShutterDenominator>0 ? stackShutterNumerator*stackFrameCount/stackShutterDenominator : 0
    readonly property string stackOutputShutterText: {
        if(!stackIsoMode || stackFrameCount<=0 || stackShutterNumerator<=0 || stackShutterDenominator<=0) return "—"
        var n=stackShutterNumerator*stackFrameCount, d=stackShutterDenominator, a=n, b=d
        while(b>0){var remainder=a%b;a=b;b=remainder}
        n/=a;d/=a
        return (d===1 ? String(n) : n+"/"+d) + " 秒"
    }
    property int stackFrameCount: 0
    property int stackGapMs: 0
    property int stackRemaining: 0
    property int pixelResolution: 6
    readonly property int frames: pixelResolution
    readonly property int captureFrames: pixelResolution === 25 ? 1 : pixelResolution
    property var cameraProxy: null
    readonly property int systemImageFormat: cameraProxy ? cameraProxy.image_format : -1
    readonly property int saveFormat: systemImageFormat === 0 || systemImageFormat === 1 || systemImageFormat === 3 ? 1 : systemImageFormat === 2 || systemImageFormat === 4 ? 0 : -1
    readonly property bool saveHeif: systemImageFormat === 3 || systemImageFormat === 4
    readonly property string saveFormatText: !stackMode && pixelResolution === 25 ? (saveHeif ? "不支持HEIF" : "沿用机内格式") : !stackMode && pixelResolution !== 6 ? "RAW＋JPEG" : saveFormat < 0 ? "—" : stackMode ? "RAW＋" + (saveHeif ? "HEIF" : "JPEG") : "JPEG" + (saveHeif ? "＋HEIF" : "") + (saveFormat === 1 ? "＋RAW" : "")
    property bool changingMode: false
    property bool applied: false
    property bool initialized: false
    // 原厂其他页面空闲时停读文件；已应用模式必须继续接收实体快门状态。
    property bool monitoringRequested: true
    readonly property bool polling: monitoringRequested || applied || busy || sending || !initialized
    property bool armReading: false
    property string armIntent: ""
    property string lastPublishedArm: ""
    property string nativeArm: ""
    property bool nativeReady: false
    property real nativeWaitPress: -1
    property bool shutterPending: false
    property int modeEpoch: 0
    property int pendingEpoch: 0
    property bool preparingMode: false
    property bool preparedMode: false
    readonly property bool quarterMode: !stackMode && pixelResolution === 25
    property bool quarterReady: false
    property bool quarterWanted: false
    property bool quarterActive: false
    property bool quarterWriting: false
    property bool quarterReading: false
    property string quarterPublished: ""
    property bool quarterDisarmed: false
    property bool quarterDisarmReading: false
    property bool normalSwitchPending: false
    readonly property bool applicationReady: quarterMode ? quarterReady && nativeReady && ready && !busy : canStart && nativeReady
    readonly property bool appliedConfirmed: quarterMode ? applied && quarterActive && quarterReady : applied && preparedMode && nativeReady && nativeArm === armLine().trim()
    property bool busy: false
    property bool ready: false
    property bool sending: false
    property bool reading: false
    property int unchangedPolls: 0
    property real heartbeat: -1
    property string stage: "offline"
    property string album: ""
    property string lastError: ""
    property int countdownRemaining: 0
    property int workDone: 0
    property int workTotal: 0
    property int renderTiles: 0
    property int jpegJoinRows: 0
    property int jpegJoinTotal: 0
    property bool jpegJoinComplete: false
    property int jpegTiles: 0
    readonly property bool canStart: initialized && ready && !busy && !sending && unchangedPolls < 12
    signal completed(string albumLocation)
    signal preparationWaitRequested()
    function setOption(key,value) {
        if (busy || sending || !Number.isInteger(value)) return false
        if (key === "initialDelay" && pixelResolution !== 25 && value >= 2 && value <= 60) {initialDelay=value;return true}
        if (key === "keepMode" && pixelResolution !== 25 && (value === 0 || value === 1)) {keepMode=value === 1;return true}
        if (key === "pixelResolution" && (value === 6 || value === 16 || value === 25)) {pixelResolution=value;return true}
        return false
    }

    function acceptStatus(value) {
        if (!value || value.protocol !== 3 || !/^[0-9a-f]{8}$/.test(value.session) ||
            !Number.isInteger(value.ack) || value.ack < 0 || value.ack > 2147483646 ||
            typeof value.busy !== "boolean" || typeof value.ready !== "boolean" ||
            !Number.isFinite(value.heartbeat)) return false
        if(quarterWanted || quarterActive) {
            ready=value.ready; initialized=true; busy=false; heartbeat=value.heartbeat;
            session=value.session;acknowledgement=value.ack;
            stage="idle";preparingMode=false;preparedMode=false;
            return true
        }
        var previous = stage
        var newComplete=value.stage === "complete" && (session !== value.session || completedAcknowledgement !== value.ack)
        var wasBusy=busy
        if(!value.stack && (value.busy || newComplete) && (value.frames===6 || value.frames===16 || value.frames===25)) {
            changingMode=true;pixelResolution=value.frames;changingMode=false
        }
        // 完成状态可能含最后一帧的新读数；也保留它们供重新打开参数页查看。
        if(value.stack === true && (value.busy || newComplete || wasBusy)){
            stackMode=true
            stackFrameCount=Number.isInteger(value.stack_frames) && value.stack_frames>=0 ? value.stack_frames : 0
            stackGapMs=Number.isInteger(value.stack_gap_ms) && value.stack_gap_ms>=0 ? value.stack_gap_ms : 0
            stackRemaining=Number.isInteger(value.stack_remaining) && value.stack_remaining>=0 ? value.stack_remaining : 0
            if(value.stack_iso_mode === true){
                stackMethod=3
                stackSourceIso=Number.isInteger(value.stack_source_iso) && value.stack_source_iso>0 && value.stack_source_iso<=65535 ? value.stack_source_iso : 0
                stackTargetIso=Number.isInteger(value.stack_target_iso) && value.stack_target_iso>0 && value.stack_target_iso<=65535 ? value.stack_target_iso : stackTargetIso
                stackPlannedFrames=Number.isInteger(value.stack_planned_frames) && value.stack_planned_frames>0 && value.stack_planned_frames<=65535 ? value.stack_planned_frames : 0
                stackShutterNumerator=Number.isInteger(value.stack_shutter_n) && value.stack_shutter_n>0 && value.stack_shutter_n<=4294967295 ? value.stack_shutter_n : 0
                stackShutterDenominator=Number.isInteger(value.stack_shutter_d) && value.stack_shutter_d>0 && value.stack_shutter_d<=4294967295 ? value.stack_shutter_d : 0
            }
        }
        countdownRemaining=Number.isInteger(value.remaining) && value.remaining>=0 && value.remaining<=60 ? value.remaining : 0
        var totalValid=value.stack === true || value.frames === 25 ? value.work_total>=5 && value.work_total<=6 : value.frames === 16 ? value.work_total === 55 : value.work_total>=16 && value.work_total<=18
        workTotal=Number.isInteger(value.work_total) && totalValid ? value.work_total : 0
        workDone=Number.isInteger(value.work_done) && value.work_done>=0 && value.work_done<=workTotal ? value.work_done : 0
        renderTiles=Number.isInteger(value.render_tiles) && value.render_tiles>=0 && value.render_tiles<=(value.frames === 16 ? 25 : 9) ? value.render_tiles : 0
        jpegTiles=Number.isInteger(value.jpeg_tiles) && value.jpeg_tiles>=0 && value.jpeg_tiles<=25 ? value.jpeg_tiles : 0
        var joinValid=value.frames===16 && value.stage==="jpeg_join" && value.jpeg_join_total===34996 &&
                Number.isInteger(value.jpeg_join_rows) && value.jpeg_join_rows>=0 && value.jpeg_join_rows<=34996
        jpegJoinRows=joinValid ? value.jpeg_join_rows : 0
        jpegJoinTotal=joinValid ? value.jpeg_join_total : 0
        jpegJoinComplete=joinValid && value.jpeg_join_complete===true && jpegJoinRows===jpegJoinTotal
        if (value.busy || newComplete) {
            if(!stackMode){
                if (pixelResolution !== 25 && Number.isInteger(value.delay) && value.delay >= 2 && value.delay <= 60) initialDelay=value.delay
                if (value.keep === 0 || value.keep === 1) keepMode=value.keep === 1
            }
        }
        if (session !== value.session) { sending = false; unchangedPolls = 0; initialized=false; applied=false; armIntent="";lastPublishedArm="";pendingEpoch=0;modeEpoch=0;preparingMode=false;preparedMode=false }
        else unchangedPolls = heartbeat === value.heartbeat ? unchangedPolls + 1 : 0
        session = value.session; heartbeat = value.heartbeat; acknowledgement = value.ack
        if(newComplete)completedAcknowledgement=value.ack
        ready = value.ready; stage = String(value.stage); busy = value.busy
        album = typeof value.album === "string" ? value.album : ""
        lastError = typeof value.error === "string" ? value.error : ""
        if(Number.isInteger(value.mode_epoch) && value.mode_epoch>=0 && value.mode_epoch<=2147483646) {
            modeEpoch=value.mode_epoch
            if(applied && pendingEpoch>modeEpoch && !busy){stage="prepare";lastError=""}
            if(applied && pendingEpoch>0 && modeEpoch>=pendingEpoch && !busy) {
                preparingMode=value.preparing === true
                preparedMode=value.prepared === true
                if(stage === "failed" || (!preparingMode && !preparedMode)) disarmMode()
            }
        }
        if(busy){applied=true;initialized=true;preparingMode=false;preparedMode=false;pendingEpoch=0}
        if(!busy && (wasBusy || newComplete)){
            initialized=true
            if(value.stage === "complete" && keepMode && !stackMode){beginPreparation()}
            else disarmMode()
        }
        if (newComplete) completed(album)
        return true
    }
    function refresh() {
        if (reading) return
        reading = true
        var request = new XMLHttpRequest()
        request.onreadystatechange = function() {
            if (request.readyState !== XMLHttpRequest.DONE) return
            reading = false
            try { if (!acceptStatus(JSON.parse(request.responseText))) ready = false }
            catch (error) { ready = false }
        }
        request.open("GET", channelDirectory + "status.json")
        request.send()
    }
    function armLine() {
        if(stackIsoMode) return "RS5 " + session + " " + (acknowledgement + 1) + " 3 " + stackTargetIso + " 0 " + stackInitialDelay + " START END\n"
        if(stackMode) return "RS4 " + session + " " + (acknowledgement + 1) + " " + stackMethod + " " + stackSeconds + " " + (stackNoise && stackMethod !== 2 ? 1 : 0) + " " + stackInitialDelay + " START END\n"
        return (pixelResolution === 6 ? "PO3 " : "PO4 ") + session + " " + (acknowledgement + 1) + " " + pixelResolution + " " + pixelDelay + " " + (keepMode ? 1 : 0) + " START END\n"
    }
    function applyStack(method,seconds,noise,initialDelay,targetIso) {
        if(initialDelay === undefined) initialDelay=2
        if(targetIso === undefined) targetIso=100
        if(busy || !Number.isInteger(method) || method<0 || method>3 ||
           !Number.isInteger(seconds) || seconds<1 || seconds>86399 ||
           !Number.isInteger(targetIso) || targetIso<1 || targetIso>65535 ||
           !Number.isInteger(initialDelay) || initialDelay<0 || initialDelay>60) return false
        changingMode=true
        if(!applied){stackFrameCount=0;stackGapMs=0;stackRemaining=seconds}
        stackSourceIso=stackPlannedFrames=0;stackShutterNumerator=stackShutterDenominator=0
        stackMode=true;stackMethod=method;stackSeconds=seconds;stackNoise=method<2 && noise;stackInitialDelay=initialDelay;stackTargetIso=targetIso
        keepMode=false
        changingMode=false
        return applyMode()
    }
    function finishStack() {
        if(!stackMode || !busy || stage!=="capture") return false
        var request=new XMLHttpRequest()
        request.open("PUT",channelDirectory+"finish.txt")
        request.send("RSF1 " + session + " " + acknowledgement + " FINISH END\n")
        return true
    }
    function quarterLine() {return quarterWanted ? "QN1 25 END\n" : "DISARMED\n"}
    function refreshQuarter() {
        if(quarterReading) return
        quarterReading=true
        var request=new XMLHttpRequest()
        request.onreadystatechange=function(){
            if(request.readyState !== XMLHttpRequest.DONE) return
            quarterReading=false
            quarterReady=request.responseText.trim() === "QUARTER_NATIVE_READY"
        }
        request.open("GET","file:///tmp/x2d-quarter-native.status");request.send()
    }
    function publishQuarter() {
        if(quarterWriting || quarterPublished === quarterLine()) return
        if(quarterWanted && (!quarterReady || !nativeReady || nativeArm !== "" || !quarterDisarmed)) return
        quarterWriting=true
        var line=quarterLine()
        var write=new XMLHttpRequest()
        write.onreadystatechange=function(){
            if(write.readyState !== XMLHttpRequest.DONE) return
            if(write.status !== 0 && write.status !== 200){quarterWriting=false;lastError="25MP输出设置未写入";return}
            var check=new XMLHttpRequest()
            check.onreadystatechange=function(){
                if(check.readyState !== XMLHttpRequest.DONE) return
                quarterWriting=false
                if(check.responseText !== line){lastError="25MP输出设置未确认";return}
                quarterPublished=line;quarterActive=line === "QN1 25 END\n"
                if(quarterActive && quarterWanted && quarterMode){applied=true;stage="idle";lastError=""}
                else if(normalSwitchPending && !quarterWanted){normalSwitchPending=false;beginNormalPreparation()}
                if(line !== quarterLine())publishQuarter()
            }
            check.open("GET",channelDirectory+"quarter.txt");check.send()
        }
        write.open("PUT",channelDirectory+"quarter.txt");write.send(line)
    }
    function refreshQuarterDisarm() {
        if(!quarterWanted || quarterDisarmReading || !nativeReady || nativeArm !== "") return
        quarterDisarmReading=true
        var request=new XMLHttpRequest()
        request.onreadystatechange=function(){
            if(request.readyState !== XMLHttpRequest.DONE) return
            quarterDisarmReading=false
            quarterDisarmed=quarterWanted && nativeReady && nativeArm === "" && request.responseText === "DISARMED\n"
            if(quarterDisarmed)publishQuarter()
        }
        request.open("GET",channelDirectory+"arm.txt");request.send()
    }
    function applyMode() {
        if(quarterMode){
            if(!applicationReady || saveHeif){lastError=saveHeif ? "25MP请使用JPEG或RAW＋JPEG" : "25MP后台尚未就绪";return false}
            keepMode=true;quarterWanted=true;normalSwitchPending=false;quarterDisarmed=false;
            preparedMode=false;preparingMode=false;pendingEpoch=0;shutterPending=false;
            armIntent="DISARMED\n";lastPublishedArm="";publishArm();publishQuarter();return true
        }
        if(applied && armIntent === armLine() && (preparingMode || preparedMode)) return true
        if (!canStart || saveFormat < 0 || !nativeReady) {lastError="快门后端尚未就绪";return false}
        beginPreparation();return true
    }
    function beginPreparation() {
        if(quarterMode){applyMode();return}
        if(quarterWanted || quarterActive || quarterPublished !== "DISARMED\n"){
            quarterWanted=false;normalSwitchPending=true;publishQuarter();return
        }
        beginNormalPreparation()
    }
    function beginNormalPreparation() {
        pendingEpoch=modeEpoch+1;preparedMode=false;preparingMode=true;stage="prepare";lastError=""
        applied=true;armIntent=armLine();lastPublishedArm="";publishArm()
    }
    function disarmMode() {
        quarterWanted=false;normalSwitchPending=false;publishQuarter()
        shutterPending=false
        pendingEpoch=0;preparingMode=false;preparedMode=false;lastPublishedArm=""
        applied=false;armIntent="DISARMED\n";publishArm()
    }
    function publishArm() {
        if (!initialized || busy || !armIntent || armIntent === lastPublishedArm) return
        var value=armIntent
        var request=new XMLHttpRequest()
        request.onreadystatechange=function(){
            if(request.readyState !== XMLHttpRequest.DONE) return
            if(request.status !== 0 && request.status !== 200){lastError="模式应用失败";applied=false;preparingMode=false;preparedMode=false;pendingEpoch=0;lastPublishedArm=""}
        }
        // 应用模式只写本机 arm 文件；只有实体快门后端写拍摄请求。
        request.open("PUT",channelDirectory+"arm.txt");request.send(value);lastPublishedArm=value
    }
    function refreshShutter() {
        if(armReading) return
        armReading=true
        var request=new XMLHttpRequest()
        request.onreadystatechange=function(){
            if(request.readyState !== XMLHttpRequest.DONE) return
            armReading=false
            try {
                var value=JSON.parse(request.responseText)
                nativeReady=value.protocol === 1 && value.ready === true
                nativeArm=nativeReady && typeof value.arm === "string" ? value.arm : ""
                if(!nativeReady || nativeArm !== "")quarterDisarmed=false
                else refreshQuarterDisarm()
                shutterPending=!quarterMode && applied && !busy && nativeReady && nativeArm === armLine().trim() && value.pending === true
                if(nativeReady && Number.isInteger(value.wait_press) && value.wait_press>=0 && value.wait_press<=4294967295){
                    var previousWait=nativeWaitPress
                    nativeWaitPress=value.wait_press
                    if(previousWait>=0 && nativeWaitPress>previousWait && applied && !busy && !appliedConfirmed)
                        preparationWaitRequested()
                }
                if(!quarterMode && nativeReady && value.error) lastError="快门请求未写入，请重新应用模式"
                if(!quarterWanted && !quarterActive && !initialized && ready){
                    initialized=true
                    if(nativeArm && nativeArm === armLine().trim()){applied=true;armIntent=armLine();lastPublishedArm=armIntent;pendingEpoch=modeEpoch;preparingMode=true}
                    else if(!busy){applied=false;armIntent="DISARMED\n";publishArm()}
                }
            } catch(error){nativeReady=false;nativeArm=""}
        }
        request.open("GET",channelDirectory+"shutter-status.json");request.send()
    }
    onInitialDelayChanged: {if(applied && !busy && !changingMode && !stackMode)beginPreparation()}
    onKeepModeChanged: {if(applied && !busy && !changingMode && !stackMode)beginPreparation()}
    onPixelResolutionChanged: {if(applied && !busy && !changingMode && !stackMode)Qt.callLater(function(){if(applied && !busy && !changingMode && !stackMode)beginPreparation()})}
    onPollingChanged: {
        // 重新打开菜单必须先取得新状态，不能使用停轮询前的就绪缓存应用模式。
        if(polling){unchangedPolls=0;ready=false;nativeReady=false}
    }
    Timer { interval: 250; repeat: true; running: root.polling; triggeredOnStart: true; onTriggered: {root.refresh();root.refreshShutter();root.refreshQuarter();root.publishQuarter()} }
}
