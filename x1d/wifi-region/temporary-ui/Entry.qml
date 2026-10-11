import QtQuick 2.5
Rectangle {
 objectName: "temporaryHotspotEntry"
 visible: parent && parent.itemValues === "generalSettingsWiFi"
 width: 238; height: 48; z: 10000
 anchors.right: parent ? parent.right : undefined; anchors.bottom: parent ? parent.bottom : undefined
 anchors.rightMargin: 20; anchors.bottomMargin: 14
 color: mouse.pressed ? "#426c86" : "#28495e"
 border.color: "#86a9bd"; radius: 4
 Text { anchors.centerIn: parent; text: "连接手机热点（临时）"; color: "white"; font.pixelSize: 21 }
 MouseArea { id: mouse; anchors.fill: parent; onClicked: wifi.action="open" }
}
