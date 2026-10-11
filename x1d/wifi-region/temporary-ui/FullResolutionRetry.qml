import QtQuick 2.5

// No image URLs or filenames are logged. Retry only the active zoom request.
Item {
    id: root
    property bool requestActive: false
    property bool storageReady: false
    property string requestIdentity: ""
    property int imageStatus: 0
    property real imageWidth: 0
    property real imageHeight: 0
    property real expectedWidth: 0
    property real expectedHeight: 0
    property int attempts: 0
    property int retryLimit: 3
    property int baseDelay: 600
    readonly property bool undersized: imageStatus===1 && expectedWidth>0 && expectedHeight>0 &&
        Math.max(imageWidth,imageHeight)<Math.max(expectedWidth,expectedHeight)*0.75
    readonly property bool needsRetry: imageStatus===3 || undersized
    signal reloadRequested()
    function consider() {
        if(!requestActive || !storageReady || !needsRetry || attempts>=retryLimit) {
            retry.stop();return
        }
        if(!retry.running) {retry.interval=baseDelay*Math.pow(2,attempts);retry.start()}
    }
    function reset() {retry.stop();attempts=0;consider()}
    onRequestIdentityChanged: reset()
    onRequestActiveChanged: reset()
    onStorageReadyChanged: consider()
    onNeedsRetryChanged: consider()
    Timer {
        id:retry;repeat:false
        onTriggered: {
            if(!root.requestActive || !root.storageReady || !root.needsRetry || root.attempts>=root.retryLimit)return
            root.attempts++
            root.reloadRequested()
            root.consider()
        }
    }
}
