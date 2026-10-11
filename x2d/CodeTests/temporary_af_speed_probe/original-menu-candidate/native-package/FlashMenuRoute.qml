import QtQuick

// 供后续原厂路由适配使用。需要显式调用 dispatch，信号旁听不能阻止原厂查表。
Item {
    id: root
    objectName: "FlashMenuRoute"
    property bool menuActive: false
    property bool mediaProcessing: false
    property bool stockSubmenuActive: false
    readonly property string extensionName: "x2dCustomFlashUi"
    readonly property bool flashShowing: host.showing
    property string iconBase: ""
    signal stockRequested(string menuName, string subMenuItem, bool openItem, bool fromShortcut)
    signal blocked()
    signal mainMenuRequested()

    // 返回 true 表示本层消费，false 表示已转发原厂处理。
    function dispatch(menuName, subMenuItem, openItem, fromShortcut) {
        if (menuName !== extensionName) {
            host.dismiss()
            stockRequested(menuName, subMenuItem, openItem, fromShortcut)
            return false
        }
        if (!menuActive || mediaProcessing || stockSubmenuActive || host.showing) {
            blocked()
            return true
        }
        if (!host.openFlash())
            blocked()
        return true
    }

    function back() { host.back() }
    function dismiss() { host.dismiss() }

    ResidentFlashHost {
        id: host
        anchors.fill: parent
        menuActive: root.menuActive
        mediaProcessing: root.mediaProcessing
        stockSubmenuActive: root.stockSubmenuActive
        iconBase: root.iconBase
        onMainMenuRequested: root.mainMenuRequested()
    }
}
