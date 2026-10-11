import QtQuick 2.5

Rectangle {
    id: hint
    objectName: "ElectronicFlashConditionsHint"
    property bool flashEnabled: false
    property bool electronic: false
    property bool manualExposure: false
    property bool exposureValid: false
    property double exposureUs: 0
    property bool displayAllowed: true
    readonly property bool withinRange: exposureValid && isFinite(exposureUs) &&
                                       exposureUs >= 250000 && exposureUs <= 500000
    // 这是用户指定的电子快门使用条件提示，不改变曝光或无线发送策略。
    visible: displayAllowed && flashEnabled && electronic && (!manualExposure || !withinRange)
    width: parent ? Math.min(parent.width - 24, 616) : 616
    height: message.implicitHeight + 18
    color: "#e6222222"
    border.color: "#ffbd59"
    radius: 6
    Text {
        id: message
        anchors.centerIn: parent
        width: parent.width - 24
        color: "#ffcf80"
        font.pixelSize: 23
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        text: "电子快门引闪：请用 M 档\n快门设为 1/4–1/2 秒"
    }
    // 无 MouseArea、焦点或按键处理，原厂拍摄和触控照常。
}
