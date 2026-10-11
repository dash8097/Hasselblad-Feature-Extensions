import QtQuick 2.5

Item {
    id: host
    objectName: "CombinedAfSettingsHost"
    property bool presented: false
    property bool preventSwipe: false
    function populateModel(unused) { presented = true }
    function closePage() { presented = false; visible = false }
    onVisibleChanged: if (!visible) presented = false
    Component.onDestruction: presented = false
    MouseArea { anchors.fill: parent; z: -1; onClicked: {} }
    SettingsPage {
        id: settingsPage
        anchors.fill: parent
        pageActive: host.presented && host.visible
        onBackRequested: host.closePage()
    }
}
