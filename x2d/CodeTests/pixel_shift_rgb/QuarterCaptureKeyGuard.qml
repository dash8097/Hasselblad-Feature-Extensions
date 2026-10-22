import QtQuick
import QtQuick.Window
import com.hasselblad.types
import com.hasselblad.keys
import "qrc:/app/qml/scripts/Keys.js" as MKeys

// 与原厂曝光页相同的按键覆盖，仅省去全屏进度绘制。
FocusScope {
    id: root
    objectName: "QuarterCaptureKeyGuard"
    property bool activeCapture: false
    property bool armedCapture: false
    property bool shutterHeld: false
    property bool halfHeld: false
    readonly property bool intercepting: activeCapture || armedCapture || shutterHeld || halfHeld
    property var previousFocus: null
    readonly property int keyOverride: activeCapture ? HblmTypes.E_CameraKeyOption_Override : 0
    visible: intercepting
    enabled: intercepting
    onInterceptingChanged: {
        if (intercepting) {
            previousFocus = root.Window.window ? root.Window.window.activeFocusItem : null
            root.forceActiveFocus()
        } else {
            root.focus = false
            var target = previousFocus
            previousFocus = null
            Qt.callLater(function() {
                if (!root.intercepting && target && target.visible && target.enabled &&
                        root.Window.window && (!root.Window.window.activeFocusItem ||
                        root.Window.window.activeFocusItem === root ||
                        root.Window.window.activeFocusItem === root.Window.window.contentItem))
                    target.forceActiveFocus()
            })
        }
    }
    Keys.onPressed: (event)=> {
        if (event.key === Qt.Key_E) {shutterHeld=true;event.accepted=true}
        // 4.2.0 半按仍由camera-service透传给原厂AF；这里仅拦截GUI的页面切换。
        else if (armedCapture && MKeys.pressedFn(KeyFn.HalfPress,event)) {halfHeld=true;event.accepted=true}
        else if (activeCapture && !MKeys.isUnblockableCustomButton(event)) event.accepted = true
    }
    Keys.onReleased: (event)=> {
        if (event.key === Qt.Key_E) {event.accepted=true;shutterHeld=false}
        else if (halfHeld && MKeys.pressedFn(KeyFn.HalfPress,event)) {event.accepted=true;halfHeld=false}
        else if (activeCapture && !MKeys.isUnblockableCustomButton(event)) event.accepted = true
    }
}
