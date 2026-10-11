"""Qt 替身曝光列表 + 真实 QML 三状态检查，不访问相机或网络。"""
import os
os.environ['QT_QPA_PLATFORM'] = 'offscreen'
os.environ['QT_QUICK_BACKEND'] = 'software'
os.environ['QML_DISABLE_DISK_CACHE'] = '1'
import sys
sys.dont_write_bytecode = True
from pathlib import Path
from PySide6.QtCore import QUrl, QMetaObject, Q_ARG, QObject, qVersion
from PySide6.QtGui import QGuiApplication
from PySide6.QtQml import QQmlApplicationEngine
from PySide6.QtQuick import QQuickItem
from PySide6.QtTest import QTest

D=Path(__file__).resolve().parent
def main():
    assert qVersion()=='6.4.1'
    app=QGuiApplication([]); engine=QQmlApplicationEngine(); warnings=[]
    engine.warnings.connect(lambda items:warnings.extend(str(i) for i in items))
    qml='''import QtQuick
import QtQuick.Window
Window {
 id: win; width:800; height:600; visible:true
 property int mode: 2
 onModeChanged: animation.preferences.mode = mode
 property alias hostState: stock.mainState
 property alias menuName: page.menuName
 property bool stockSoundEnabled:true
 property int stockVolume:3
 function refresh() { settings.refresh() }
 QtObject { id:stock; property string mainState:"liveview" }
 Item {
  id:page; objectName:"SettingsGeneric_root"; anchors.fill:parent
  property string menuName:"exposureMenu"
  property real heightForItem:88
  ListView {
   objectName:"SettingsGeneric_list"; anchors.fill:parent
   model:ListModel { ListElement { label:"stock" } }
   delegate:Text { text:label; height:88 }
  }
 }
 X2dShutterAnimation {
  id:animation; stateSource:stock; networkEnabled:false
  Component.onCompleted: preferences.ready=true
 }
 X2dExposureSettings { id:settings; searchRoot:page; preferences:animation.preferences }
}'''
    engine.loadData(qml.encode(),QUrl.fromLocalFile(str(D/'settings-fixture.qml')))
    assert engine.rootObjects(),warnings
    win=engine.rootObjects()[0];QTest.qWait(300)
    option=win.findChild(QQuickItem,'X2dExposureEffectOption')
    assert option is not None and option.property('available')
    animation=win.findChild(QQuickItem,'X2dShutterAnimation')
    for mode in [0,1,2]:
        win.setProperty('mode',mode)
        win.setProperty('hostState','exposing');QTest.qWait(30)
        assert animation.property('showing') == (mode>0)
        win.setProperty('hostState','liveview')
        assert not animation.property('showing')
        assert win.property('stockSoundEnabled') and win.property('stockVolume')==3
    win.setProperty('menuName','focusMenu');QMetaObject.invokeMethod(win,'refresh');QTest.qWait(30)
    assert win.findChild(QQuickItem,'X2dExposureEffectOption') is None
    win.setProperty('menuName','exposureMenu');QMetaObject.invokeMethod(win,'refresh');QTest.qWait(30)
    assert win.findChild(QQuickItem,'X2dExposureEffectOption') is not None
    assert not warnings,warnings
    win.close()
    print('PASS: exposure-only attachment, mode 0/1/2 visual gating, factory fields unchanged, detach/reattach; desktop substitute only.')

if __name__=='__main__': main()
