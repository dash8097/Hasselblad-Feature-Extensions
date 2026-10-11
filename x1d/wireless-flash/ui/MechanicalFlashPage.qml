import QtQuick 2.5

Rectangle {
    id: panel
    objectName: "MechanicalFlashPage"
    color: "#141414"
    property real unit: Math.min(width / 640, height / 480)
    property bool pageActive: false
    signal backRequested()
    onPageActiveChanged: {
        if (pageActive) mainRoot.hblRfPrepare()
        else mainRoot.hblRfPageClosed()
    }
    MouseArea { anchors.fill: parent }
    Column {
        x: 16 * panel.unit; y: 12 * panel.unit
        width: parent.width - 32 * panel.unit
        spacing: 10 * panel.unit
        Row {
            width: parent.width; height: 38 * panel.unit; spacing: 14 * panel.unit
            Rectangle {
                width: 164 * panel.unit; height: parent.height; color: "#303030"; radius: 4
                Text { anchors.centerIn: parent; text: "‹ 拍摄参数"; color: "white"; font.pixelSize: 21 * panel.unit }
                MouseArea { anchors.fill: parent; onClicked: panel.backRequested() }
            }
            Text { text: "无线引闪  ·  2 / 2"; color: "white"; font.pixelSize: 24 * panel.unit; anchors.verticalCenter: parent.verticalCenter }
        }
        Row {
            width: parent.width; height: 48 * panel.unit; spacing: 14 * panel.unit
            Rectangle {
                width: 225 * panel.unit; height: parent.height; radius: 5
                color: mainRoot.hblRfEnabled ? "#805829" : "#303030"
                Text { anchors.centerIn: parent; text: "自动引闪：" + (mainRoot.hblRfEnabled ? "开" : "关"); color: "white"; font.pixelSize: 24 * panel.unit }
                MouseArea {
                    anchors.fill: parent
                    onClicked: if (mainRoot.hblRfEnabled || (mainRoot.hblRfReady && !mainRoot.hblRfBusy)) mainRoot.hblRfConfigure(!mainRoot.hblRfEnabled)
                }
            }
            Text {
                width: parent.width - 239 * panel.unit; anchors.verticalCenter: parent.verticalCenter
                text: "开启后每拍最多一次\n切换信号、延迟保持开启"; color: "#C9C9C9"; font.pixelSize: 18 * panel.unit
            }
        }
        Grid {
            width: parent.width; columns: 4; spacing: 6 * panel.unit
            Repeater {
                model: ["A 首次同步", "B 首次同步", "启动保持", "退出空闲", "状态 1 置位", "状态 3 置位", "返回空闲"]
                delegate: Rectangle {
                    width: (panel.width - 50 * panel.unit) / 4; height: 48 * panel.unit; radius: 4
                    color: mainRoot.hblRfSource === index ? "#805829" : "#303030"
                    border.color: mainRoot.hblRfSource === index ? "#E3A156" : "#303030"
                    Text { anchors.centerIn: parent; text: modelData; color: "white"; font.pixelSize: 18 * panel.unit }
                    MouseArea { anchors.fill: parent; onClicked: mainRoot.hblRfSelectSource(index) }
                }
            }
            Text { width: (panel.width-50*panel.unit)/4; height: 48*panel.unit; text: "CH 5 · ID 5\nD 组"; color: "#AAA"; font.pixelSize: 16*panel.unit; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        }
        Row {
            width: parent.width; height: 50 * panel.unit; spacing: 8 * panel.unit
            Text { width: 200*panel.unit; anchors.verticalCenter: parent.verticalCenter; text: "延迟  " + (mainRoot.hblRfDelayUs/1000).toFixed(2) + " ms"; color: "#E3A156"; font.pixelSize: 25*panel.unit }
            Repeater {
                model: ["−"+mainRoot.hblRfStepUs, "归零", "+"+mainRoot.hblRfStepUs]
                delegate: Rectangle {
                    width: (panel.width-256*panel.unit)/3; height: 50*panel.unit; color: "#303030"; radius: 4
                    Text { anchors.centerIn: parent; text: modelData; color: "white"; font.pixelSize: 24*panel.unit }
                    MouseArea { anchors.fill: parent; onClicked: mainRoot.hblRfAdjustDelay(index===1 ? 0 : mainRoot.hblRfDelayUs+(index===0 ? -1:1)*mainRoot.hblRfStepUs) }
                }
            }
        }
        Row {
            width: parent.width; height: 36*panel.unit; spacing: 8*panel.unit
            Text { width: 200*panel.unit; anchors.verticalCenter: parent.verticalCenter; text: "调节步长"; color: "#AAA"; font.pixelSize: 18*panel.unit }
            Repeater {
                model: [10,100,1000]
                delegate: Rectangle {
                    width: (panel.width-256*panel.unit)/3; height: 36*panel.unit; radius: 4
                    color: mainRoot.hblRfStepUs===modelData ? "#68451D" : "#252525"
                    Text { anchors.centerIn: parent; text: modelData+" us"; color: "white"; font.pixelSize: 19*panel.unit }
                    MouseArea { anchors.fill: parent; onClicked: mainRoot.hblRfStepUs=modelData }
                }
            }
        }
        Row {
            width: parent.width; height: 46*panel.unit; spacing: 8*panel.unit
            Text { width: 200*panel.unit; anchors.verticalCenter: parent.verticalCenter; text: "增益索引  " + mainRoot.hblRfPowerIndex; color: "#CCC"; font.pixelSize: 19*panel.unit }
            Repeater {
                model: ["增强", "减弱"]
                delegate: Rectangle {
                    width: 82*panel.unit; height: 46*panel.unit; color: "#303030"; radius: 4
                    Text { anchors.centerIn: parent; text: modelData; color: "white"; font.pixelSize: 19*panel.unit }
                    MouseArea { anchors.fill: parent; onClicked: mainRoot.hblRfSetPower(Math.max(10,Math.min(100,mainRoot.hblRfPowerIndex+(index===0 ? -5:5)))) }
                }
            }
            Rectangle {
                width: 204*panel.unit; height: 46*panel.unit; color: "#68451D"; radius: 4
                Text { anchors.centerIn: parent; text: mainRoot.hblRfBusy ? "处理中…" : mainRoot.hblRfReady ? "单次试闪" : "尚未就绪"; color: "white"; font.pixelSize: 21*panel.unit }
                MouseArea { anchors.fill: parent; onClicked: mainRoot.hblRfTestOnce() }
            }
        }
        Text {
            width: parent.width; height: 42*panel.unit; wrapMode: Text.WordWrap
            text: mainRoot.hblRfStatus
            color: "#BBB"; font.pixelSize: 17*panel.unit
        }
    }
}
