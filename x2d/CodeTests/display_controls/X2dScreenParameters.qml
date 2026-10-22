import QtQuick
import QtQuick.Window
import com.hasselblad.proxies
import com.hasselblad.types
import com.hasselblad.keys
import com.hasselblad.constants
import "qrc:/app/qml/popups"

// 4.2.0：只复用原厂 EVF-only liveview 和眼感选择，不写熄屏/帧率参数。
Item {
    id: root
    objectName: "X2dScreenParameters"
    property var searchRoot: null
    property bool menusActive: false
    readonly property var liveviewController: liveviewTimeout.item
    property var keyOwner: null
    property bool guarding: false
    property bool ownsInput: false
    property bool halfHeld: false
    property var stateModel:null
    property bool evfRequested:false
    readonly property bool evfEntryWanted:modeEnabled && (System.screens_active & HblmTypes.E_Screens_EVF)!==0
    readonly property bool modeEnabled: Camera.custom_option_liveview_evf_only &&
        (System.screens_allowed & HblmTypes.E_Screens_Back) !== 0 &&
        (System.screens_allowed & HblmTypes.E_Screens_EVF) !== 0
    readonly property bool backActive: (System.screens_active & HblmTypes.E_Screens_Back) !== 0 &&
        (System.screens_active & HblmTypes.E_Screens_EVF) === 0
    readonly property bool intercept: modeEnabled && backActive
    parent: root.Window.window ? root.Window.window.contentItem : null
    anchors.fill: parent
    z: 2000
    Loader {
        id: liveviewTimeout
        source: "X2dScreenLiveviewTimeout.qml"
        onLoaded: {
            item.searchRoot = Qt.binding(function() { return root.searchRoot })
            item.menusActive = Qt.binding(function() { return root.menusActive })
            root.queueMenuRefresh()
        }
    }

    function findStateModel(item) {
        if(!item || item===root) return null
        if(item.objectName==="StateMachineViewModel_root") return item
        var children=item.children || []
        for(var i=0;i<children.length;++i) {var found=findStateModel(children[i]);if(found)return found}
        return null
    }
    function enterEvfLiveview() {
        if(!evfEntryWanted) {evfRequested=false;return}
        if(!stateModel && root.Window.window) stateModel=findStateModel(root.Window.window.contentItem)
        if(!stateModel || !stateModel.screenEVF || evfRequested) return
        if(stateModel.isShutdown || stateModel.isMaintenanceUpgrade || stateModel.isMaintenanceManual || stateModel.exposing) return
        // 调用原厂页面状态机的正常取景入口；不发半按、AF 或拍摄命令。
        evfRequested=true
        stateModel.setLiveView(true)
        console.info("X2D_SCREEN_PARAMETERS_EVF_LIVEVIEW_REQUESTED")
    }

    function releaseKeys() {
        if(keyOwner) {
            var keep=[]
            for(var i=0;i<keyOwner.Keys.forwardTo.length;++i)
                if(keyOwner.Keys.forwardTo[i]!==root) keep.push(keyOwner.Keys.forwardTo[i])
            keyOwner.Keys.forwardTo=keep
        }
        keyOwner=null
    }
    function attachKeys() {
        var next=intercept && root.Window.window ? root.Window.window.activeFocusItem : null
        if(next===root || next===keyOwner) return
        releaseKeys()
        if(!next) return
        var previous=[]
        for(var i=0;i<next.Keys.forwardTo.length;++i) previous.push(next.Keys.forwardTo[i])
        next.Keys.forwardTo=[root].concat(previous)
        keyOwner=next
    }
    function enforceForward() {
        if(!intercept || guarding) return
        ownsInput=true
        var value=Camera.forward_input_events
        var bit=HblmTypes.E_CameraKeyOption_OverrideHalfPress
        var wanted=originalForward() | bit
        if(value!==wanted) {
            guarding=true
            Camera.forward_input_events=wanted
            guarding=false
        }
    }
    function originalForward() {
        var item=root.Window.window ? root.Window.window.activeFocusItem : null
        while(item) {
            if(item!==root && "keyOverride" in item && item.keyOverride) return item.keyOverride
            item=item.parent
        }
        return 0
    }
    function restoreForward() {
        if(!ownsInput || guarding) return
        ownsInput=false
        guarding=true
        Camera.forward_input_events=originalForward()
        guarding=false
    }
    function refreshMode() {
        halfHeld=false; notice.stop()
        if(intercept) { attachKeys(); enforceForward() }
        else {
            releaseKeys()
            restoreForward()
        }
    }
    Keys.onPressed: (event)=> {
        if(intercept && KeyFn.isFn(event.key,event.modifiers,KeyFn.HalfPress)) {
            if(!halfHeld && !event.isAutoRepeat) { halfHeld=true; notice.restart() }
            event.accepted=true
        } else event.accepted=false
    }
    Keys.onReleased: (event)=> {
        if(intercept && KeyFn.isFn(event.key,event.modifiers,KeyFn.HalfPress)) {
            halfHeld=false; event.accepted=true
        } else event.accepted=false
    }
    PopupIconText {
        id: bubble
        objectName: "X2dScreenParametersNotice"
        anchors.fill: parent
        visible: root.intercept && notice.running
        // 原厂 TimeoutText 使用的原始内容组件，直接复用尺寸、图标和排版。
        // 不创建 TimeoutText 外围的 Background，亦不抢页面焦点或触摸。
        text:qsTranslate("TimeoutText","Live View in EVF Only")
    }
    Timer { id:notice; interval:1000 }
    function refreshMenus(item) {
        if(!item || item===root) return
        // 替换指定原厂行的 Loader，保持 SettingsGeneric 参数和其他行不变。
        if(item.objectName==="specialItem" && item.item && item.item.model) {
            var name=item.item.model.itemName
            var next=name==="screens_allowed" ? "X2dDisplayModeDelegate.qml" :
                     name==="screen_idle_timeout" ? "X2dScreenTimeoutDelegate.qml" :
                     name==="allow_variable_fps" ? "X2dAdaptiveFpsDelegate.qml" : ""
            if(next && String(item.source).indexOf(next)<0) item.source=Qt.resolvedUrl(next)
        }
        var children=item.children || []
        for(var i=0;i<children.length;++i) refreshMenus(children[i])
    }
    // 只在树结构、Loader 或页面模型改变时重新定位；稳定菜单不周期遍历。
    property var menuWatches: []
    function queueMenuRefresh() { Qt.callLater(refreshMenuTree) }
    function refreshMenuTree() {
        var nodes=[]
        function collect(item) {
            if(!item || item===root) return
            nodes.push(item)
            var children=item.children || []
            for(var i=0;i<children.length;++i) collect(children[i])
        }
        if(menusActive) collect(searchRoot)
        var next=[]
        for(var i=0;i<nodes.length;++i) {
            var watcher=null
            for(var j=0;j<menuWatches.length;++j)
                if(menuWatches[j].target===nodes[i]) {watcher=menuWatches[j];break}
            next.push(watcher || menuWatch.createObject(root,{target:nodes[i]}))
        }
        for(var k=0;k<menuWatches.length;++k)
            if(next.indexOf(menuWatches[k])<0) {menuWatches[k].target=null;menuWatches[k].destroy()}
        menuWatches=next
        if(menusActive) refreshMenus(searchRoot)
        if(liveviewTimeout.item) liveviewTimeout.item.refreshMenus()
    }
    Component {
        id:menuWatch
        Connections {
            ignoreUnknownSignals:true
            function onChildrenChanged() {root.queueMenuRefresh()}
            function onLoaded() {root.queueMenuRefresh()}
            function onItemChanged() {root.queueMenuRefresh()}
            function onModelChanged() {root.queueMenuRefresh()}
            function onMenuNameChanged() {root.queueMenuRefresh()}
            function onObjectNameChanged() {root.queueMenuRefresh()}
        }
    }
    onSearchRootChanged: queueMenuRefresh()

    Connections {
        target:root.Window.window
        function onActiveFocusItemChanged() { root.attachKeys(); Qt.callLater(root.enforceForward) }
    }
    Connections {
        target:Camera
        function onForward_input_eventsChanged() { root.enforceForward() }
    }
    Connections {
        target:root.stateModel
        function onScreenEVFChanged() {Qt.callLater(root.enterEvfLiveview)}
        function onExposingChanged() {Qt.callLater(root.enterEvfLiveview)}
    }
    onEvfEntryWantedChanged:Qt.callLater(enterEvfLiveview)
    onInterceptChanged:refreshMode()
    onMenusActiveChanged: queueMenuRefresh()
    Component.onCompleted: {refreshMode();Qt.callLater(enterEvfLiveview);queueMenuRefresh()}
    Component.onDestruction: { releaseKeys(); restoreForward() }
}
