import QtQuick 2.0

// Light photo-viewfinder presentation. All values are supplied by the native
// camera controller; this item never starts capture, AF, or video processing.
Item {
    id: hud
    objectName: "OwnViewfinderHud"
    property real unit: Math.min(width / 640, height / 480)
    property string apertureText: ""
    property string shutterText: ""
    property string isoText: ""
    property string remainingText: ""
    property string focusText: ""
    property string whiteBalanceText: ""
    property bool wifiActive: false
    property bool gpsActive: false
    property bool flashReady: false
    property bool chargeActive: false
    property bool batteryVisible: true
    property bool aeLocked: false
    property bool electronicShutter: false
    property bool exposureScaleValid: false
    property real exposureValue: 0
    property int batteryLevel: 0
    property real remainingOpacity: 1
    property string textFont: "sans-serif"
    readonly property real topHeight: Math.round(49 * unit)
    readonly property real bottomHeight: Math.round(62 * unit)

    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: hud.topHeight
        color: "#a0000000"
    }
    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: hud.bottomHeight
        color: "#a0000000"
    }
    Rectangle { y: hud.topHeight - 1; width: parent.width; height: 1; color: "#666666" }
    Rectangle { y: parent.height - hud.bottomHeight; width: parent.width; height: 1; color: "#666666" }

    Row {
        x: 15 * hud.unit
        height: hud.topHeight
        spacing: 13 * hud.unit
        Text {
            anchors.verticalCenter: parent.verticalCenter
            color: "white"; font.family: hud.textFont
            font.pixelSize: 23 * hud.unit
            text: hud.whiteBalanceText
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            color: "white"; font.family: hud.textFont
            font.pixelSize: 23 * hud.unit
            text: hud.focusText
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            visible: hud.wifiActive
            color: "white"; font.family: hud.textFont
            font.pixelSize: 19 * hud.unit
            text: "Wi-Fi"
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            visible: hud.flashReady
            color: "white"; font.family: hud.textFont
            font.pixelSize: 19 * hud.unit
            text: "⚡"
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            visible: hud.gpsActive
            color: "white"; font.family: hud.textFont
            font.pixelSize: 19 * hud.unit
            text: "GPS"
        }
    }
    Row {
        anchors.right: parent.right
        anchors.rightMargin: 15 * hud.unit
        height: hud.topHeight
        spacing: 9 * hud.unit
        Text {
            anchors.verticalCenter: parent.verticalCenter
            color: "white"; font.family: hud.textFont
            font.pixelSize: 24 * hud.unit
            text: hud.isoText
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            visible: hud.chargeActive
            text: "⚡"; color: "white"
            font.family: hud.textFont; font.pixelSize: 19 * hud.unit
        }
        Item {
            anchors.verticalCenter: parent.verticalCenter
            visible: hud.batteryVisible
            width: 31 * hud.unit; height: 18 * hud.unit
            Rectangle {
                width: parent.width - 3 * hud.unit; height: parent.height
                color: "transparent"; border.color: "white"; border.width: Math.max(1, hud.unit)
                radius: 2 * hud.unit
            }
            Rectangle {
                anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
                width: 3 * hud.unit; height: 9 * hud.unit; color: "white"
            }
            Rectangle {
                x: 3 * hud.unit; y: 3 * hud.unit
                height: parent.height - 6 * hud.unit
                width: (parent.width - 9 * hud.unit) * Math.max(0, Math.min(100, hud.batteryLevel)) / 100
                color: hud.batteryLevel < 15 ? "#ff654f" : "white"
            }
        }
    }

    Row {
        x: 16 * hud.unit
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 11 * hud.unit
        spacing: 8 * hud.unit
        Text {
            anchors.verticalCenter: parent.verticalCenter
            visible: hud.aeLocked
            color: "white"; font.family: hud.textFont
            font.pixelSize: 18 * hud.unit
            text: "AE-L"
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            visible: hud.electronicShutter
            color: "white"; font.family: hud.textFont
            font.pixelSize: 18 * hud.unit
            text: "E"
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            color: "white"; font.family: hud.textFont
            font.pixelSize: 29 * hud.unit
            text: hud.apertureText
        }
    }
    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 11 * hud.unit
        color: "white"; font.family: hud.textFont
        font.pixelSize: 29 * hud.unit
        text: hud.shutterText
    }
    Text {
        anchors.right: parent.right
        anchors.rightMargin: 16 * hud.unit
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 12 * hud.unit
        width: 150 * hud.unit
        horizontalAlignment: Text.AlignRight
        fontSizeMode: Text.HorizontalFit
        opacity: hud.remainingOpacity
        color: "white"; font.family: hud.textFont
        font.pixelSize: 27 * hud.unit
        text: hud.remainingText
    }

    Item {
        id: scale
        objectName: "OwnViewfinderExposureScale"
        visible: hud.exposureScaleValid
        width: 226 * hud.unit
        height: 35 * hud.unit
        x: 18 * hud.unit
        y: parent.height - hud.bottomHeight - height - 4 * hud.unit
        Rectangle {
            x: -4 * hud.unit
            y: -22 * hud.unit
            width: parent.width + 8 * hud.unit
            height: parent.height + 26 * hud.unit
            color: "#66000000"
        }
        Repeater {
            model: 13
            delegate: Rectangle {
                x: index * (scale.width - width) / 12
                anchors.bottom: parent.bottom
                width: Math.max(1, hud.unit)
                height: (index % 3 === 0 ? 12 : 7) * hud.unit
                color: "white"
            }
        }
        Rectangle {
            x: (Math.max(-3, Math.min(3, hud.exposureValue)) + 3) * (scale.width - width) / 6
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 2 * hud.unit
            width: 3 * hud.unit; height: 25 * hud.unit
            color: "white"
        }
        Text {
            anchors.bottom: parent.top
            anchors.bottomMargin: -2 * hud.unit
            color: "white"; font.family: hud.textFont
            font.pixelSize: 17 * hud.unit
            text: (hud.exposureValue > 0 ? "+" : "") + hud.exposureValue.toFixed(1)
        }
    }
}
