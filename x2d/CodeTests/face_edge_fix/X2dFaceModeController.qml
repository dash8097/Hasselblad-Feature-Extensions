import QtQuick
import com.hasselblad.types
import com.hasselblad.proxies

// 4.2.0 原厂 EyeDetection 工程选项的普通菜单入口；不声称改变推理模型。
Item {
    id: root
    objectName: "X2dFaceModeController"
    property var searchRoot: null
    property var camera: Camera
    property var system: System
    property int multi: HblmTypes.E_FaceDetection_Multi
    property int off: HblmTypes.E_FaceDetection_Off
    property int eyeBit: HblmTypes.E_DebugOption_EyeDetection
    readonly property int currentMode: camera.face_detection === off ? 2 :
        (system.debug_mode && (system.debug_options & eyeBit) ? 1 : 0)
    readonly property bool busy: phase !== ""
    property string phase: ""
    property string errorText: ""
    property int wanted: 0
    property int remaining: 0
    property int desiredOptions: 0
    property bool ownsMaster: false
    property int originalOtherOptions: 0
    property var routedLoader: null
    signal applied()

    function fail() {
        phase = ""
        errorText = "设置未完成，请检查当前状态后重试"
    }
    function waitFor(next) { phase = next; remaining = 30 }
    function writeOptions() {
        // 总开关原来关闭时，先等它回读为开启，再读取原来被隐藏的位。
        var old = system.debug_options
        if (phase === "master-on") originalOtherOptions = old & ~eyeBit
        desiredOptions = wanted === 1 ? old | eyeBit : old & ~eyeBit
        waitFor("options")
        system.debug_options = desiredOptions
    }
    function writeFace() {
        waitFor("face")
        camera.face_detection = wanted === 2 ? off : multi
    }
    function choose(mode, allowed) {
        if (busy || !allowed || [0,1,2].indexOf(mode) < 0) return false
        wanted = mode
        errorText = ""
        if (wanted === 1 && !system.debug_mode) {
            ownsMaster = true
            waitFor("master-on")
            system.debug_mode = true
        } else if (system.debug_mode) {
            writeOptions()
        } else {
            writeFace()
        }
        return true
    }
    function advance() {
        if (!busy) return
        if (--remaining < 0) { fail(); return }
        if (phase === "master-on" && system.debug_mode) {
            writeOptions()
        } else if (phase === "options" && system.debug_options === desiredOptions) {
            if (wanted !== 1 && ownsMaster &&
                (system.debug_options & ~eyeBit) === originalOtherOptions) {
                waitFor("master-off")
                system.debug_mode = false
            } else writeFace()
        } else if (phase === "master-off" && !system.debug_mode) {
            ownsMaster = false
            writeFace()
        } else if (phase === "face" && camera.face_detection === (wanted === 2 ? off : multi)) {
            if (currentMode !== wanted) { fail(); return }
            phase = ""
            applied()
        }
    }
    function find(item) {
        if (!item || item === root) return null
        if (item.objectName === "PopoverFaceDetection_root") return item
        var a = item.children || []
        for (var i=0; i<a.length; ++i) { var v=find(a[i]); if (v) return v }
        return null
    }
    function findPopupLoader(item) {
        if (!item || item === root) return null
        if (item.objectName === "ControlScreen_popupLoader") return item
        var children=item.children || []
        for (var i=0; i<children.length; ++i) {
            var loader=findPopupLoader(children[i]); if(loader) return loader
        }
        return null
    }
    function redirectFacePopup() {
        if (!routedLoader) return
        if (String(routedLoader.source).endsWith("/PopoverFaceDetection.qml"))
            routedLoader.source=Qt.resolvedUrl("X2dFaceModePopup.qml")
        if (routedLoader.item && routedLoader.item.objectName === "X2dFaceModePopup")
            routedLoader.item.controller=root
    }
    function attachMenu() {
        if (!routedLoader) routedLoader=findPopupLoader(searchRoot)
        redirectFacePopup()
    }
    Connections {
        target:root.routedLoader
        function onSourceChanged() { root.redirectFacePopup() }
        function onLoaded() { root.redirectFacePopup() }
    }
    Timer { interval: 100; running: root.busy; repeat: true; onTriggered: root.advance() }
    Timer { interval: 150; running: root.searchRoot !== null && root.routedLoader === null; repeat: true; onTriggered: root.attachMenu() }
}
