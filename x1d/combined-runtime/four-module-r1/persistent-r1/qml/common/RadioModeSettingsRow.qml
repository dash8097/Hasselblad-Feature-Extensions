import QtQuick 2.5

Item {
    id: row
    objectName: "WifiThreeStateRow"
    property string title: "Wi-Fi"
    property string fontName: "Helvetica Neue LT Std"
    property real textSize: 24
    property real labelMargin: 20
    property real switchSpacing: 20
    property color textColor: "white"
    function gotSelected() { modeSwitch.advance() }
    Text {
        id: label
        anchors.left: parent.left;anchors.right: parent.horizontalCenter
        anchors.rightMargin: row.labelMargin;anchors.verticalCenter: parent.verticalCenter
        text: row.title+":";color: row.textColor
        font.family: row.fontName;font.pixelSize: row.textSize
        horizontalAlignment: Text.AlignRight;verticalAlignment: Text.AlignVCenter
        fontSizeMode: Text.HorizontalFit
    }
    RadioModeButton {
        id: modeSwitch;objectName: "WifiModeSwitch"
        anchors.left: label.right;anchors.leftMargin: row.switchSpacing
        anchors.verticalCenter: parent.verticalCenter
        width: 90;height: 67
    }
}
