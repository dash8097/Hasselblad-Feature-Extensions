import QtQuick 2.5

Item {
    id: row
    width: 608; height: 64
    property string label
    property bool checked: true
    property string fontName: "Helvetica Neue LT Std"
    signal toggled()
    FlashStyle { id: flashStyle }
    FlashText {
        x: 20; width: parent.width-124; height: parent.height
        text: row.label; font.family: row.fontName; font.pixelSize: flashStyle.titleSize
    }
    Rectangle {
        x: parent.width-84; y: (parent.height-height)/2; width: 64; height: 30
        radius: 15; color: row.checked ? flashStyle.activeColor : "transparent"
        border.color: "white"; border.width: 1
        Rectangle { x: row.checked ? 34 : 0; y: 0; width: 30; height: 30; radius: 15; color: "white" }
    }
    MouseArea { anchors.fill: parent; onClicked: row.toggled() }
}
