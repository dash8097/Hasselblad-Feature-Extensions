import QtQuick

// 插在原厂曝光设置的列表页尾，复用该列表的滚动与页面生命周期。
FocusScope {
    id: root
    objectName: "X2dExposureEffectOption"
    property var preferences: null
    property real rowHeight: 88
    property real textSize: 28
    property real inset: 30
    width: parent ? parent.width : 800
    height: rowHeight * 2
    readonly property bool available: preferences && preferences.ready && !preferences.saving
    function choose(index) { if (available) preferences.selectMode(index) }
    Rectangle { anchors.fill: parent; color: "black" }
    Text {
        x: root.inset; y: 12; color: "white"; font.pixelSize: root.textSize
        text: "曝光动画与声音"
    }
    Row {
        x: root.inset; y: root.rowHeight * .7
        width: root.width - 2 * root.inset; spacing: 8
        Repeater {
            model: ["关", "动画", "动画与声音"]
            delegate: Rectangle {
                required property int index
                required property string modelData
                objectName: "X2dEffectChoice" + index
                width: (root.width - 2 * root.inset - 16)/3
                height: root.rowHeight * .75
                radius: 4; color: "#151515"
                border.color: root.preferences && root.preferences.mode === index ? "#e6a02b" : "#666666"
                opacity: root.available ? 1 : .4
                Text { anchors.centerIn: parent; text: modelData; color: "white"; font.pixelSize: root.textSize * .8 }
                MouseArea { anchors.fill: parent; enabled: root.available; onClicked: { root.forceActiveFocus(); root.choose(index) } }
            }
        }
    }
    Text {
        x: root.inset; anchors.bottom: parent.bottom; color: "#bbbbbb"; font.pixelSize: root.textSize * .6
        text: preferences ? preferences.errorText : "效果设置暂不可用"
    }
    Keys.onLeftPressed: if (available) choose(Math.max(0, preferences.mode-1))
    Keys.onRightPressed: if (available) choose(Math.min(2, preferences.mode+1))
}
