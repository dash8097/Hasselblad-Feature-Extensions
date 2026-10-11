import QtQuick 2.5

Item {
    id: gate
    visible: false
    property var nativeAdapter: null
    property bool contextValid: false
    property var contextOwner: null
    property bool electronic: false
    property double exposureUs: 0
    property bool pending: false
    property int pendingToken: 0
    property int activeToken: 0
    property int nextToken: 0
    property var savedOwner: null
    property bool savedElectronic: false
    property double savedExposureUs: 0
    property bool savedLiveview: false
    property bool alive: true
    readonly property int continueToken: nativeAdapter ? nativeAdapter.continueToken : 0
    readonly property int cancelToken: nativeAdapter ? nativeAdapter.cancelToken : 0
    signal continueExposure(bool liveviewAfterExposure)
    signal cancelled()
    function command(value) {if(nativeAdapter)nativeAdapter.command=JSON.stringify(value)}
    function begin(liveviewAfterExposure) {
        if(!alive || pending || activeToken || !contextValid)return false
        if(!nativeAdapter || !nativeAdapter.enabled) {
            continueExposure(liveviewAfterExposure)
            return true
        }
        var candidate=Math.max(nextToken,nativeAdapter.lastToken)+1
        if(!nativeAdapter.ready || !isFinite(exposureUs) || exposureUs<1 ||
                exposureUs>86400000000 || candidate>=2147483647)return false
        nextToken=candidate;pendingToken=candidate;pending=true
        savedOwner=contextOwner;savedElectronic=electronic;savedExposureUs=exposureUs
        savedLiveview=liveviewAfterExposure
        timeout.start()
        command({op:"begin",token:candidate,electronic:electronic,exposureUs:Math.round(exposureUs)})
        return true
    }
    function cancel() {
        if(!pending)return
        var token=pendingToken;pending=false;pendingToken=0;savedOwner=null;timeout.stop()
        command({op:"cancel",token:token});cancelled()
    }
    function shotEnded() {
        if(!activeToken)return
        var token=activeToken;activeToken=0
        command({op:"shotEnd",token:token})
    }
    function continueIfCurrent() {
        if(!pending || continueToken!==pendingToken)return
        var ok=alive && contextValid && contextOwner===savedOwner &&
            electronic===savedElectronic && exposureUs===savedExposureUs &&
            nativeAdapter && nativeAdapter.connected && nativeAdapter.enabled
        if(!ok) {command({op:"continueAck",token:pendingToken,ok:false});cancel();return}
        var token=pendingToken;var live=savedLiveview
        activeToken=token;pending=false;pendingToken=0;savedOwner=null;timeout.stop()
        command({op:"continueAck",token:token,ok:true})
        continueExposure(live)
    }
    onContinueTokenChanged: continueIfCurrent()
    onCancelTokenChanged: if(pending && cancelToken===pendingToken)cancel()
    onContextValidChanged: if(!contextValid)cancel()
    onContextOwnerChanged: if(pending && contextOwner!==savedOwner)cancel()
    onElectronicChanged: if(pending && electronic!==savedElectronic)cancel()
    onExposureUsChanged: if(pending && exposureUs!==savedExposureUs)cancel()
    property bool nativeAlive: nativeAdapter ? nativeAdapter.connected && nativeAdapter.enabled : false
    onNativeAliveChanged: if(!nativeAlive)cancel()
    Timer {id: timeout;interval: 30000;repeat: false;onTriggered: gate.cancel()}
    Component.onDestruction: {alive=false;cancel()}
}
