import QtQuick
import "flash-ui" as Flash

// 离线候选：须由原厂菜单适配层提供状态，尚未接入相机。
// 页面是直接子对象，不使用随菜单 active/source 改变而卸载的 Loader。
FocusScope {
    id: root
    objectName: "ResidentFlashHost"
    property bool menuActive: false
    property bool mediaProcessing: false
    property bool stockSubmenuActive: false
    property bool flashOpen: false
    property string iconBase: ""
    property string fontName: "Avenir Next"
    readonly property alias page: flash
    readonly property bool showing: menuActive && flashOpen && !stockSubmenuActive
    visible: showing
    enabled: showing
    focus: showing
    signal mainMenuRequested()

    function openFlash() {
        if (!menuActive || mediaProcessing || stockSubmenuActive)
            return false
        flash.closeDetail()
        flashOpen = true
        forceActiveFocus()
        return true
    }
    function dismiss() {
        flashOpen = false
        flash.closeDetail()
    }
    function back() {
        if (showing)
            flash.requestBack()
    }
    onMenuActiveChanged: { if (!menuActive) dismiss() }
    onStockSubmenuActiveChanged: { if (stockSubmenuActive) dismiss() }
    Keys.onPressed: (event) => {
        // 这里只验证通用返回键。原厂 KeyFn 和快门路由由后续适配层负责。
        if (showing && (event.key === Qt.Key_Escape || event.key === Qt.Key_Back)) {
            back()
            event.accepted = true
        } else {
            event.accepted = false
        }
    }
    Flash.FlashPage {
        id: flash
        anchors.fill: parent
        pageActive: root.showing
        iconBase: root.iconBase
        fontName: root.fontName
        connected: false
        masterEnabled: false
        canTest: false
        onBackRequested: {
            root.dismiss()
            root.mainMenuRequested()
        }
        // 不接任何射频、相机、镜头或持久设置信号。
    }
}
