import QtQuick 2.5

// 离线生命周期候选；未接入原厂 MediaBrowseView。
// 调用方先填纯 URL，再打开 presented；退出先关闭 presented。
Item {
    id: root
    property bool presented: false
    property url selectedSource: ""
    property alias imageStatus: pixels.status
    property alias imageSource: pixels.source
    visible: presented
    enabled: presented

    onPresentedChanged: {
        if (!presented)
            selectedSource = ""
    }

    Image {
        id: pixels
        objectName: "residentPixels"
        anchors.fill: parent
        asynchronous: true
        cache: false
        source: root.presented ? root.selectedSource : ""
    }
}
