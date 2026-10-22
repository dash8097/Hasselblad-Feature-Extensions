import QtQuick
import QtQuick.Window
import com.hasselblad.proxies
import com.hasselblad.types

Item {
    id: root
    objectName: "X2dScreenLiveviewTimeout"
    property var searchRoot: null
    property bool menusActive: false
    property var stateModel: null
    property var drawer: null
    property var multishot: null
    property int attempts: 0
    property var targetList: null
    property var previousModel: null
    property var previousDelegate: null
    property var adapter: null
    property bool rowReady: false
    property alias preferences: preferences
    parent: root.Window.window ? root.Window.window.contentItem : null
    anchors.fill: parent
    z: 2001
    readonly property int language: System.language_index
    readonly property string errorLabel: language === HblmTypes.E_Language_SimplifiedChinese ? "保存失败，请重试" :
        language === HblmTypes.E_Language_TraditionalChinese ? "儲存失敗，請重試" :
        language === HblmTypes.E_Language_Japanese ? "保存できませんでした" :
        language === HblmTypes.E_Language_Korean ? "저장하지 못했습니다" :
        language === HblmTypes.E_Language_Spanish ? "No se pudo guardar" :
        language === HblmTypes.E_Language_French ? "Échec de l’enregistrement" :
        language === HblmTypes.E_Language_German ? "Speichern fehlgeschlagen" :
        language === HblmTypes.E_Language_Italian ? "Salvataggio non riuscito" :
        language === HblmTypes.E_Language_Swedish ? "Kunde inte spara" :
        language === HblmTypes.E_Language_Russian ? "Не удалось сохранить" : "Could not save"
    readonly property string title: {
        switch (language) {
        case HblmTypes.E_Language_SimplifiedChinese: return "取景超时"
        case HblmTypes.E_Language_TraditionalChinese: return "取景逾時"
        case HblmTypes.E_Language_Spanish: return "Tiempo de vista en vivo"
        case HblmTypes.E_Language_French: return "Délai de visée"
        case HblmTypes.E_Language_German: return "Live-View-Zeitlimit"
        case HblmTypes.E_Language_Italian: return "Tempo Live View"
        case HblmTypes.E_Language_Swedish: return "Tidsgräns för direktvisning"
        case HblmTypes.E_Language_Russian: return "Время визирования"
        case HblmTypes.E_Language_Japanese: return "ライブビュー時間"
        case HblmTypes.E_Language_Korean: return "라이브 뷰 시간 제한"
        default: return "Live view timeout"
        }
    }
    function valueLabel(value) {
        switch (value) {
        case 0: return qsTranslate("IdleTimeout", "Never")
        case 10: return qsTranslate("IdleTimeout", "10 s")
        case 20: return qsTranslate("IdleTimeout", "20 s")
        case 30: return qsTranslate("IdleTimeout", "30 s")
        case 60: return qsTranslate("IdleTimeout", "1 m")
        case 120: return qsTranslate("IdleTimeout", "2 m")
        case 300: return qsTranslate("IdleTimeout", "5 m")
        default: return "—"
        }
    }
    function find(node, name) {
        if (!node || node === root) return null
        if (node.objectName === name) return node
        var children = node.children || []
        for (var i = 0; i < children.length; ++i) { var found = find(children[i], name); if (found) return found }
        return null
    }
    function findPower(node) {
        if (!node || node === root) return null
        if (String(node.objectName).indexOf("SettingsGeneric_root") === 0 && node.menuName === "powerMenu") return node
        var children = node.children || []
        for (var i = 0; i < children.length; ++i) { var found = findPower(children[i]); if (found) return found }
        return null
    }
    function findMultishot(node) {
        if (!node || node === root) return null
        if ("activeController" in node) return node.activeController
        var children = node.children || []
        for (var i = 0; i < children.length; ++i) { var found = findMultishot(children[i]); if (found) return found }
        return null
    }
    function discover() {
        stateModel = find(searchRoot, "StateMachineViewModel_root")
        drawer = find(searchRoot, "ControlDrawer_root")
        multishot = findMultishot(searchRoot)
    }
    function detach() {
        rowReady = false
        if (targetList && targetList.model === adapter) {
            targetList.model = previousModel; targetList.delegate = previousDelegate
        }
        targetList = null; previousModel = null; previousDelegate = null
        if (adapter) { adapter.destroy(); adapter = null }
    }
    function refreshMenus() {
        discover()
        var page = menusActive ? findPower(searchRoot) : null
        var list = find(page, "SettingsGeneric_list")
        if (list !== targetList) {
            detach()
            if (!list || !list.model || !list.delegate || typeof list.model.srcIndex !== "function") return
            previousModel = list.model; previousDelegate = list.delegate; targetList = list
            adapter = modelFactory.createObject(root, {sourceModel: previousModel, stockDelegate: previousDelegate})
            list.model = adapter
        }
        replaceRow(page)
    }
    function replaceRow(node) {
        if (!node || node === root) return
        if (node.objectName === "specialItem" && node.item && node.item.model && node.item.model.itemName === "x2d_liveview_seconds") {
            if (String(node.source).indexOf("X2dScreenLiveviewDelegate.qml") < 0) {
                rowReady = false; node.source = Qt.resolvedUrl("X2dScreenLiveviewDelegate.qml")
            } else if ("timeoutHost" in node.item && node.item.timeoutHost === root) rowReady = true
        }
        var children = node.children || []
        for (var i = 0; i < children.length; ++i) replaceRow(children[i])
    }
    Component {
        id: modelFactory
        X2dScreenLiveviewModel {
            label: root.title
            seconds: preferences.seconds
            available: root.rowReady && preferences.ready && !preferences.saving
        }
    }
    X2dScreenLiveviewPreferences { id: preferences }
    X2dScreenLiveviewPolicy {
        id: policy
        seconds: preferences.ready ? preferences.seconds : 0
        touching: touch.active
        eligible: root.Window.window !== null && root.Window.window.visible && !root.menusActive &&
            root.stateModel !== null && root.drawer !== null && root.drawer.mainState === "liveview" &&
            root.stateModel.liveviewState === HblmTypes.E_LiveViewState_Active &&
            root.stateModel.exposureStatus === 0 && !root.stateModel.inSession && !root.stateModel.activeError &&
            !Camera.lens_ring_rotation &&
            !root.stateModel.isShutdown && !root.stateModel.isUpgrading && !root.stateModel.isFormatting &&
            !root.stateModel.isMaintenanceManual && !root.stateModel.isMaintenanceUpgrade &&
            !root.stateModel.isTethered && !System.media_processing && Storage.storage_status === 0 &&
            root.multishot !== null && !root.multishot.busy && (!root.multishot.applied || root.multishot.quarterMode) &&
            !root.multishot.preparingMode && !root.multishot.sending
        // 使用原厂页面状态机，让停止取景与显示参数页一起执行。
        onExpired: if (root.stateModel) root.stateModel.toggle_screen("control_screen", {})
    }
    // PointHandler 被动观察，既不设置 exclusive grab，也不接管触摸目标。
    PointHandler { id: touch; target: null; acceptedButtons: Qt.AllButtons; onPointChanged: policy.activity() }
    Connections {
        target: Camera
        function onLens_ring_rotationChanged() { policy.activity() }
        function onFocus_pointChanged() { policy.activity() }
    }
    Connections {
        target: root.stateModel
        function onKeyPressed() { policy.activity() }
        function onCustomButton() { policy.activity() }
        function onCustomWheel() { policy.activity() }
        function onFocusModeChanged() { policy.activity() }
    }
    Timer {
        interval: 250; repeat: true
        running: root.searchRoot !== null && (root.stateModel === null || root.drawer === null) && root.attempts < 80
        onTriggered: { root.attempts++; root.discover() }
    }
    onSearchRootChanged: { detach(); attempts = 0; discover() }
    onMenusActiveChanged: refreshMenus()
    Component.onDestruction: detach()
}
