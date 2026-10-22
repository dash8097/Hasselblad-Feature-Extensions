import QtQuick

// 原常驻服务 + 原持久标记；不另建 Qt Settings/数据库，不写原厂属性。
Item {
    id: root
    visible: false
    property string serviceUrl: "http://127.0.0.1:38491"
    property bool networkEnabled: true
    property int mode: 0
    property bool ready: false
    property bool saving: false
    property string errorText: ""
    property int attempts: 0
    readonly property var labels: ["关", "动画", "动画与声音"]
    function requestMode(next) {
        if (!networkEnabled || saving || (next !== -1 && (!ready || next < 0 || next > 2))) return
        saving = true
        var xhr = new XMLHttpRequest()
        xhr.onreadystatechange = function() {
            if (xhr.readyState !== XMLHttpRequest.DONE) return
            saving = false
            if (xhr.status === 200 && /^[012]$/.test(xhr.responseText)) {
                mode = Number(xhr.responseText)
                ready = true
                errorText = ""
            } else {
                errorText = next < 0 ? "效果设置暂不可用" : "保存未确认，请重新读取"
                // An uncertain write can have reached storage; block further choices until read back.
                ready = false
                if (next >= 0) attempts = 0
            }
        }
        xhr.open(next < 0 ? "GET" : "POST", serviceUrl + (next < 0 ? "/mode" : "/mode/" + next))
        xhr.send()
    }
    function selectMode(next) { requestMode(next) }
    Timer {
        interval: 300; repeat: true
        running: root.networkEnabled && !root.ready && root.attempts < 34
        onTriggered: { if (!root.saving) { root.attempts++; root.requestMode(-1) } }
    }
    Component.onCompleted: if (networkEnabled) requestMode(-1)
}
