import QtQuick 2.5

// 只续行本次已通过原厂检查的全按；本组件没有相机对象。
Item {
    id: gate
    objectName: "FormalExposureGate"
    visible: false
    property var nativeAdapter: null
    property bool contextValid: false
    property var contextOwner: null
    property bool electronic: false
    property double exposureUs: 0
    property bool pending: false
    property int nextToken: 0
    property int pendingToken: 0
    property int activeToken: 0
    property bool savedLiveview: false
    property bool savedElectronic: false
    property double savedExposureUs: 0
    property var savedOwner: null
    property bool alive: true
    readonly property int ackToken: nativeAdapter ? nativeAdapter.flushAckToken : 0
    signal continueExposure(bool liveviewAfterExposure)
    signal cancelled()

    function command(value) { if (nativeAdapter) nativeAdapter.command=JSON.stringify(value) }
    function begin(liveviewAfterExposure) {
        if (!alive || pending || activeToken || !contextValid) return false
        // 未启用时保留原厂同步调用行为；开启请求尚未确认时不误拍。
        if (!nativeAdapter || (!nativeAdapter.masterRequested && !nativeAdapter.masterEnabled)) {
            continueExposure(liveviewAfterExposure)
            return true
        }
        var lastNativeToken=nativeAdapter.lastFlushToken
        if (lastNativeToken===undefined) lastNativeToken=0
        var candidate=Math.max(nextToken,lastNativeToken)+1
        if (!nativeAdapter.connected || !nativeAdapter.masterEnabled || !isFinite(exposureUs) ||
                exposureUs<1 || exposureUs>86400000000 || candidate>=2147483647) return false
        nextToken=candidate; pendingToken=candidate
        savedOwner=contextOwner; savedElectronic=electronic; savedExposureUs=exposureUs
        savedLiveview=liveviewAfterExposure; pending=true
        timeout.start()
        command({op:"flush",token:pendingToken,electronic:electronic,exposureUs:Math.round(exposureUs)})
        return true
    }
    function cancel() {
        if (!pending) return
        var token=pendingToken
        pending=false; pendingToken=0; savedOwner=null; timeout.stop()
        command({op:"cancel",token:token})
        cancelled()
    }
    function shotEnded() {
        if (!activeToken) return
        var token=activeToken; activeToken=0
        command({op:"shotEnd",token:token})
    }
    function checkAcknowledgement() {
        if (!pending || !nativeAdapter || ackToken!==pendingToken) return
        if (nativeAdapter.flushResult!==0 || !nativeAdapter.connected || !nativeAdapter.masterEnabled ||
                !alive || !contextValid || contextOwner!==savedOwner ||
                electronic!==savedElectronic || exposureUs!==savedExposureUs) { cancel(); return }
        var live=savedLiveview
        activeToken=pendingToken; pending=false; pendingToken=0; savedOwner=null; timeout.stop()
        continueExposure(live)
    }
    onAckTokenChanged: checkAcknowledgement()
    onContextValidChanged: if (!contextValid) cancel()
    onContextOwnerChanged: if (pending && contextOwner!==savedOwner) cancel()
    onElectronicChanged: if (pending && electronic!==savedElectronic) cancel()
    onExposureUsChanged: if (pending && exposureUs!==savedExposureUs) cancel()
    property bool nativeAlive: nativeAdapter ? nativeAdapter.connected && nativeAdapter.masterEnabled : false
    onNativeAliveChanged: if (!nativeAlive) cancel()
    Timer { id: timeout; interval: 6500; repeat: false; onTriggered: gate.cancel() }
    Component.onDestruction: { alive=false; cancel(); shotEnded() }
}
