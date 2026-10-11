import QtQuick 2.0

// 仅供已审计的 Menu / SettingsGeneric 容器使用。active 表示呈现，原生 Loader 保留实例。
FocusScope {
    id: host
    property url residentSource
    property url source
    property bool active: false
    property bool asynchronous: true
    property var initialProperties: ({})
    property int generation: 0
    property int deliveredGeneration: -1
    property bool readyToRun: false
    property bool transitioning: false
    property var presentedItem: null
    readonly property bool useResident: source.toString() === residentSource.toString()
    readonly property var item: active ? presentedItem : null
    readonly property int status: !active ? Loader.Null :
                                 (useResident ? retained.status : lazyPageLoader.status)
    enabled: active
    signal loaded()

    function retire() {
        transitioning = true
        if (presentedItem && typeof presentedItem.residentDeactivate === "function")
            presentedItem.residentDeactivate()
        presentedItem = null
        transitioning = false
    }
    function invalidate() {
        if (!readyToRun) return
        generation++
        retire()
        lazyPageLoader.active = false
        if (active) dispatch.restart()
        else dispatch.stop()
    }
    function setSource(url, properties) {
        // 先断开旧页面的可见性监听，再改变 source；不依赖 Qt 的绑定求值顺序。
        retire()
        initialProperties = properties || {}
        source = url
        invalidate()
    }
    function dispatchOpen() {
        if (!active || deliveredGeneration === generation) return
        var selected
        if (useResident) {
            if (retained.status !== Loader.Ready) return
            selected = retained.item
            for (var key in initialProperties) {
                // 原生 Loader 已按容器尺寸布局；旧调用传入的字符串不是合法 anchor 对象。
                if (key !== "anchors.fill" && key !== "focus")
                    selected[key] = initialProperties[key]
            }
            selected.residentActivate()
        } else {
            if (!lazyPageLoader.active) {
                lazyPageLoader.setSource(source, initialProperties)
                lazyPageLoader.active = true
            }
            if (lazyPageLoader.status !== Loader.Ready) return
            selected = lazyPageLoader.item
        }
        presentedItem = selected
        deliveredGeneration = generation
        loaded()
    }
    onSourceChanged: invalidate()
    onActiveChanged: invalidate()
    Component.onCompleted: {
        // 必须在构造参数中关闭业务连接，不能等 onLoaded 后才关闭。
        retained.setSource(residentSource, {"residentPresented": false, "residentHost": host})
        readyToRun = true
        invalidate()
    }
    Timer { id: dispatch; interval: 0; onTriggered: host.dispatchOpen() }
    Loader {
        id: retained
        objectName: "residentNativeLoader"
        anchors.fill: parent
        active: true
        asynchronous: true
        visible: host.active && host.useResident && host.presentedItem === item
        enabled: visible
        focus: visible
        onLoaded: if (host.active) dispatch.restart()
    }
    Loader {
        id: lazyPageLoader
        objectName: "transientNativeLoader"
        anchors.fill: parent
        active: false
        asynchronous: host.asynchronous
        visible: host.active && !host.useResident
        enabled: visible
        focus: visible
        onLoaded: dispatch.restart()
    }
}
