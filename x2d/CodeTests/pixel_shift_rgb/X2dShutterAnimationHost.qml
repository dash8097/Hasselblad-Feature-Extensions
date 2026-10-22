import QtQuick
import QtQuick.Window

Item {
    id: root
    property var searchRoot:null
    property var drawer:null
    property bool menuActive:false
    property bool networkEnabled:true
    property alias preferences:settings
    readonly property bool animationEnabled:settings.ready && settings.mode>0
    readonly property bool soundEnabled:settings.ready && settings.mode===2
    X2dEffectPreferences {id:settings;networkEnabled:root.networkEnabled}
    X2dExposureSettings {searchRoot:root.searchRoot;preferences:settings;active:root.menuActive}
    Loader {
        objectName:"X2dShutterLoader"
        parent:root.Window.window ? root.Window.window.contentItem : root.searchRoot
        anchors.fill:parent;z:1000
        active:root.animationEnabled
        source:"X2dShutterAnimation.qml"
        onLoaded:{item.stateSource=Qt.binding(function(){return root.drawer});item.externalPreferences=settings;item.networkEnabled=root.networkEnabled}
    }
}
