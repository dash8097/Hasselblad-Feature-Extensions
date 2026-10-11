import QtQuick 2.0

// 固定目录中的普通页按 key 保留；其余 source 继续使用原生临时 Loader。
FocusScope {
    id: host
    property url residentSource
    property var catalog: []
    property string selectionKey: ""
    property bool warmEnabled: false
    property url source
    property bool active: false
    property bool asynchronous: true
    property var initialProperties: ({})
    property var poolLoaders: ({})
    property int generation: 0
    property int deliveredGeneration: -1
    property bool readyToRun: false
    property bool transitioning: false
    property var presentedItem: null
    readonly property bool useResident: source.toString() === residentSource.toString() && findEntry(selectionKey) !== null
    readonly property var selectedLoader: useResident && poolLoaders[selectionKey] ? poolLoaders[selectionKey] : null
    readonly property var item: active ? presentedItem : null
    readonly property int status: !active ? Loader.Null :
                                 (useResident ? (selectedLoader ? selectedLoader.status : Loader.Loading) : lazyPageLoader.status)
    readonly property int residentCount: Object.keys(poolLoaders).length
    readonly property int readyCount: countReady()
    enabled: active
    signal loaded()

    function findEntry(key) {
        for (var i = 0; i < catalog.length; i++)
            if (catalog[i].key === key) return catalog[i]
        return null
    }
    function countReady() {
        var count = 0
        for (var key in poolLoaders)
            if (poolLoaders[key].status === Loader.Ready) count++
        return count
    }
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
        retire()
        initialProperties = properties || {}
        source = url
        invalidate()
    }
    function ensureEntry(entry) {
        if (poolLoaders[entry.key]) return poolLoaders[entry.key]
        var loader = slotFactory.createObject(host, {"slotKey": entry.key})
        if (!loader) return null
        var next = {}
        for (var key in poolLoaders) next[key] = poolLoaders[key]
        next[entry.key] = loader
        poolLoaders = next
        var properties = {"residentPresented": false, "residentHost": host}
        for (var name in entry.properties) properties[name] = entry.properties[name]
        loader.setSource(residentSource, properties)
        return loader
    }
    function warmNext() {
        if (!readyToRun || !warmEnabled) return
        for (var i = 0; i < catalog.length; i++) {
            var loader = poolLoaders[catalog[i].key]
            if (!loader) {
                ensureEntry(catalog[i])
                return
            }
            if (loader.status === Loader.Loading) return
        }
    }
    function dispatchOpen() {
        if (!active || deliveredGeneration === generation) return
        var selected
        if (useResident) {
            var loader = ensureEntry(findEntry(selectionKey))
            if (!loader || loader.status !== Loader.Ready) return
            selected = loader.item
            for (var key in initialProperties) {
                // 旧调用中的字符串 anchors.fill 不是合法 anchor 对象。
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
    onSelectionKeyChanged: invalidate()
    onActiveChanged: invalidate()
    onWarmEnabledChanged: {
        if (warmEnabled) warmDispatch.restart()
        else warmDispatch.stop()
    }
    onCatalogChanged: if (readyToRun) warmDispatch.restart()
    Component.onCompleted: {
        readyToRun = true
        invalidate()
        warmDispatch.restart()
    }
    Timer { id: dispatch; interval: 0; onTriggered: host.dispatchOpen() }
    Timer { id: warmDispatch; interval: 16; onTriggered: host.warmNext() }
    Component {
        id: slotFactory
        Loader {
            property string slotKey
            objectName: "residentNativeLoader"
            anchors.fill: parent
            active: true
            asynchronous: true
            visible: host.active && host.useResident && host.presentedItem === item
            enabled: visible
            focus: visible
            onLoaded: {
                item.residentPrepare()
                if (host.active) dispatch.restart()
                warmDispatch.restart()
            }
            onStatusChanged: if (status === Loader.Error) warmDispatch.restart()
        }
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
