import QtQuick 2.5

Item {
    id: gate
    objectName: "FormalExposureGate"
    visible: false
    property var nativeAdapter: null
    property bool contextValid: false
    property var contextOwner: null
    property bool electronic: false
    property double exposureUs: 0
    property bool halfPowerEnabled: false
    property bool alive: true
    property bool held: false
    property bool fullCommitted: false
    property bool prepared: false
    property bool pending: false
    property int activeToken: 0
    property int nextToken: 0
    property bool preparedElectronic: false
    property double preparedExposure: 0
    property var preparedOwner: null
    readonly property bool available: nativeAdapter && nativeAdapter.connected && nativeAdapter.masterEnabled
    readonly property int ackToken: nativeAdapter ? nativeAdapter.flushAckToken : 0
    signal continueExposure(bool liveviewAfterExposure)
    signal cancelled()

    function command(value) { if (nativeAdapter) nativeAdapter.command=JSON.stringify(value) }
    function validExposure() { return isFinite(exposureUs) && exposureUs>=1 && exposureUs<=86400000000 }
    function startRequest(op) {
        if(activeToken || !available || !validExposure())return false
        var last=nativeAdapter.lastFlushToken
        if(last===undefined)last=0
        var token=Math.max(nextToken,last)+1
        if(token>=2147483647)return false
        nextToken=token;activeToken=token
        prepared=op==="prepare"
        preparedElectronic=electronic;preparedExposure=exposureUs;preparedOwner=contextOwner
        command({op:op,token:token,electronic:electronic,exposureUs:Math.round(exposureUs)})
        return activeToken===token
    }
    function prepareHalfPress() {
        if(held)return
        held=true;fullCommitted=false
        if(!alive || !contextValid || !halfPowerEnabled)return
        startRequest("prepare")
    }
    function begin(liveviewAfterExposure) {
        if(!alive || !contextValid)return false
        if(prepared && (preparedElectronic!==electronic || preparedOwner!==contextOwner))shotEnded()
        if(!fullCommitted && prepared && activeToken && available && validExposure())
            command({op:"queueSync",token:activeToken,electronic:electronic,exposureUs:Math.round(exposureUs)})
        else if(!activeToken)startRequest("flush")
        fullCommitted=true
        // 原厂曝光不等待无线队列或回执。
        continueExposure(liveviewAfterExposure)
        return true
    }
    function halfReleased() {
        held=false
        if(!fullCommitted)shotEnded()
        fullCommitted=false
    }
    function shotEnded() {
        var token=activeToken
        activeToken=0;prepared=false;preparedOwner=null
        if(token)command({op:"shotEnd",token:token})
    }
    function cancel() {
        // 原厂全按释放也调用 cancel；不能取消已经放行的正常曝光。
        if(prepared && !fullCommitted) {shotEnded();cancelled()}
    }
    function checkAcknowledgement() {
        if(activeToken && ackToken===activeToken && nativeAdapter.flushResult!==0) {
            activeToken=0;prepared=false;preparedOwner=null
        }
    }
    onAckTokenChanged: checkAcknowledgement()
    onHalfPowerEnabledChanged: if(!halfPowerEnabled)cancel()
    onContextValidChanged: if(!contextValid)cancel()
    onContextOwnerChanged: cancel()
    onElectronicChanged: cancel()
    onAvailableChanged: if(!available)shotEnded()
    Component.onDestruction: {alive=false;shotEnded()}
}
