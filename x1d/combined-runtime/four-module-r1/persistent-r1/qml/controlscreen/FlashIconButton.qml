import QtQuick 2.5

Item {
    id: button
    property url source
    property string label: ""
    property bool available: true
    property real iconOpacity: 1
    property real iconSize: 30
    property string fontName: "Helvetica Neue LT Std"
    property bool outlined: false
    property bool groupSelector: false
    signal clicked()
    FlashStyle { id: flashStyle }
    opacity: available ? 1 : flashStyle.disabledOpacity
    Rectangle {
        anchors.fill: parent; anchors.margins: 1
        color: "transparent"; border.color: "white"; border.width: 1
        visible: button.outlined
    }
    Image {
        anchors.horizontalCenter: parent.horizontalCenter
        y: button.label.length ? 23-height/2 : (parent.height-height)/2
        width: button.iconSize; height: button.iconSize
        source: button.source; fillMode: Image.PreserveAspectFit
        opacity: button.iconOpacity * (hit.pressed ? 0.55 : 1)
        visible: !button.groupSelector
    }
    Item {
        anchors.horizontalCenter: parent.horizontalCenter
        y: 8; width: 34; height: 30; visible: button.groupSelector
        opacity: hit.pressed ? 0.55 : 1
        Rectangle { x: 0; y: 4; width: 21; height: 2; color: "white" }
        Rectangle { x: 0; y: 13; width: 21; height: 2; color: "white" }
        Rectangle { x: 0; y: 22; width: 21; height: 2; color: "white" }
        Rectangle { x: 24; y: 12; width: 10; height: 2; color: "white" }
        Rectangle { x: 28; y: 8; width: 2; height: 10; color: "white" }
    }
    FlashText {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom; anchors.bottomMargin: 5
        text: button.label; color: "white"
        font.family: button.fontName; font.pixelSize: flashStyle.captionSize
    }
    MouseArea {
        id: hit; anchors.fill: parent; enabled: button.available
        onClicked: button.clicked()
    }
}
