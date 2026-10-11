import QtQuick
import QtQuick.Window
import "flash-ui"

Window {
    id: root
    objectName: "x2dCustomMainMenu"
    // Creation never maps the surface; the resident controller shows it later.
    visible: false
    visibility: Window.Hidden
    flags: Qt.FramelessWindowHint
    width: Screen.width
    height: Screen.height
    color: "black"
    // Eagle shell uses this exact title to classify interactive GUI surfaces.
    // Match the original image-preview window; a descriptive title loses focus/layer routing.
    title: "gui"

    readonly property real sx: width / 1024
    readonly property real sy: height / 768
    readonly property real sf: Math.min(sx, sy)
    property int selectedIndex: -1
    property string menuFont: "Avenir Next"
    property bool flashPageOpen: false
    property int exitSerial: 0
    property int frameSerial: 0
    property bool frameReported: false
    onFrameSwapped: {
        if (visible && !frameReported) {
            frameReported = true
            console.info("X2D_MENU_FRAME " + (++frameSerial))
        }
    }
    onVisibleChanged: {
        frameReported = false
        if (!visible) flashPageOpen = false
        else inputScope.forceActiveFocus()
    }
    function requestExit() {
        // Read by our resident controller; this page never calls camera APIs.
        console.info("X2D_MENU_EXIT_REQUEST " + (++exitSerial))
    }
    function inputAction(event) {
        if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back) return "back"
        if (event.key === Qt.Key_Left) return "left"
        if (event.key === Qt.Key_Right) return "right"
        if (event.key === Qt.Key_Up) return "up"
        if (event.key === Qt.Key_Down) return "down"
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) return "select"
        return "consume"
    }
    function handleInput(action) {
        if (action === "exit") { requestExit(); return }
        if (action === "back") {
            if (flashPageOpen) flashPageOpen = false
            else if (selectedIndex >= 0) selectedIndex = -1
            else requestExit()
            return
        }
        if (flashPageOpen || action === "consume") return
        if (selectedIndex < 0) { selectedIndex = 0; return }
        if (action === "left") selectedIndex = (selectedIndex + 11) % 12
        else if (action === "right") selectedIndex = (selectedIndex + 1) % 12
        else if (action === "up") selectedIndex = (selectedIndex + 8) % 12
        else if (action === "down") selectedIndex = (selectedIndex + 4) % 12
        else if (action === "select") {
            if (selectedIndex === 11) flashPageOpen = true
            else settingsRequested(entries[selectedIndex].key)
        }
    }
    FocusScope {
        id: inputScope
        anchors.fill: parent
        focus: true
        Keys.onPressed: (event)=> {
            root.handleInput(root.inputAction(event))
            event.accepted = true
        }
        Keys.onReleased: (event)=> { event.accepted = true }
    }
    // Cover gaps between tiles as well as the tiles. Child controls are above it.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        preventStealing: true
        onWheel: (wheel)=> { wheel.accepted = true }
    }
    // Local navigation contract only. No camera commands are issued by this QML.
    signal settingsRequested(string menuName)

    readonly property var entries: [
        { key: "exposureMenu", title: qsTranslate("MENUS", "Exposure") },
        { key: "focusMenu", title: qsTranslate("MENUS", "Focus") },
        { key: "qualityMenu", title: qsTranslate("MENUS", "Quality") },
        { key: "cropModesMenu", title: qsTranslate("MENUS", "Crop Modes") },
        { key: "flashMenu", title: qsTranslate("MENUS", "Flash") },
        { key: "displayMenu", title: qsTranslate("MENUS", "Display") },
        { key: "powerMenu", title: qsTranslate("MENUS", "Power") },
        { key: "storageMenu", title: qsTranslate("MENUS", "Storage") },
        { key: "ibisMenu", title: qsTranslate("MENUS", "Stabilisation") },
        { key: "wifiMenu", title: qsTranslate("MENUS", "Wi-Fi") },
        { key: "generalMenu", title: qsTranslate("MENUS", "General") },
        { key: "customTest", title: "引闪" }
    ]

    Item {
        id: grid
        visible: !root.flashPageOpen
        readonly property real frameWidth: 136 * root.sx
        readonly property real frameHeight: 102 * root.sy
        readonly property real cellWidth: ((root.width - 4 * root.sx) - 160 * root.sx - 4 * frameWidth) / 3 + frameWidth
        readonly property real cellHeight: 220.8 * root.sy
        width: 4 * cellWidth
        height: 3 * cellHeight
        y: 102 * root.sy
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.horizontalCenterOffset: 2 * root.sx

        Repeater {
            model: root.entries
            delegate: Item {
                id: tile
                required property int index
                required property var modelData
                objectName: "menuTile_" + modelData.key
                x: (index % 4) * grid.cellWidth
                y: Math.floor(index / 4) * grid.cellHeight
                width: grid.cellWidth
                height: grid.cellHeight

                Image {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: (grid.frameHeight - height) / 2
                    // The factory SVG provider interprets 160 as a percentage.
                    width: Math.round((modelData.key === "cropModesMenu" ? 63.125 : 64) * 1.6 * root.sf)
                    height: Math.round((modelData.key === "cropModesMenu" ? 54.375 : 64) * 1.6 * root.sf)
                    sourceSize.width: width
                    sourceSize.height: height
                    source: "assets/" + modelData.key + ".svg"
                    asynchronous: false
                }
                Text {
                    x: 8 * root.sx
                    y: grid.frameHeight + 16 * root.sy
                    width: parent.width - 16 * root.sx
                    text: modelData.title
                    color: "white"
                    font.family: root.menuFont
                    font.pixelSize: 33.6 * root.sf
                    font.bold: touch.pressed
                    font.letterSpacing: touch.pressed ? -0.72 : 0
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                }
                MouseArea {
                    id: touch
                    objectName: "menuTouch_" + modelData.key
                    anchors.fill: parent
                    onPressed: root.selectedIndex = tile.index
                    onClicked: {
                        if (modelData.key === "customTest")
                            root.flashPageOpen = true
                        else
                            root.settingsRequested(modelData.key)
                    }
                }
            }
        }
    }
    FlashPage {
        id: flashPage
        anchors.fill: parent
        visible: root.flashPageOpen
        pageActive: visible && root.visible
        fontName: root.menuFont
        connected: false
        masterEnabled: false
        canTest: false
        stateText: "界面预览"
        onBackRequested: root.flashPageOpen = false
        // This instance intentionally has no native/radio/camera adapter.
    }
}
