"""Qt 6.4.1 离线渲染与状态联动检查。需 PySide6 6.4.1，不连接相机。"""
import os
os.environ['QT_QPA_PLATFORM'] = 'offscreen'
os.environ['QT_QUICK_BACKEND'] = 'software'
os.environ['QML_DISABLE_DISK_CACHE'] = '1'
import sys
sys.dont_write_bytecode = True
from pathlib import Path
from PySide6.QtCore import QUrl, qVersion, QObject
from PySide6.QtGui import QGuiApplication
from PySide6.QtQml import QQmlApplicationEngine
from PySide6.QtQuick import QQuickItem
from PySide6.QtTest import QTest

D = Path(__file__).resolve().parent


def main():
    assert qVersion() == '6.4.1', qVersion()
    app = QGuiApplication([])
    engine = QQmlApplicationEngine()
    warnings = []
    engine.warnings.connect(lambda items: warnings.extend(str(item) for item in items))
    fixture = '''import QtQuick
import QtQuick.Window
Window {
 id: window; width:800; height:600; visible:true
 property alias testState: stock.mainState
 QtObject { id:stock; property string mainState:"liveview" }
 Loader { anchors.fill:parent; sourceComponent: Component { X2dShutterAnimation { networkEnabled:false } } onLoaded:{ item.preferences.ready=true; item.stateSource=stock } }
}'''
    engine.loadData(fixture.encode(), QUrl.fromLocalFile(str(D/'animation-test.qml')))
    assert engine.rootObjects(), warnings
    window = engine.rootObjects()[0]
    QTest.qWait(180)
    overlay = window.findChild(QQuickItem, 'X2dShutterAnimation')
    assert overlay is not None
    assert overlay.property('warmed'), 'Canvas must be primed before first capture'
    assert not overlay.property('showing') and overlay.opacity() == 0
    window.setProperty('testState', 'exposing')
    QTest.qWait(270)
    assert overlay.property('showing') and overlay.property('playCount') == 1
    assert abs(overlay.property('closure')-1) < 0.01
    QTest.qWait(400)
    assert overlay.property('closure') == 0, 'Blades must be open before the final star-only interval'
    assert abs(overlay.property('apertureRadius')-524.2640687) < .001, 'Open polygon must reach all four screen corners'
    assert 0 < overlay.property('flight') < 1
    QTest.qWait(180)
    assert overlay.property('progress') == 1
    assert overlay.property('showing'), 'Wait for stock state; timer cannot control camera'
    window.setProperty('testState', 'liveview')
    assert not overlay.property('showing')
    # Early restoration cancels immediately and the same instance restarts next shot.
    for count in range(2, 12):
        window.setProperty('testState', 'exposing')
        QTest.qWait(15)
        assert overlay.property('playCount') == count
        window.setProperty('testState', 'liveview')
        assert not overlay.property('showing')
        assert window.findChild(QQuickItem, 'X2dShutterAnimation') is overlay
    window.setProperty('testState', 'exposing')
    QTest.qWait(2100)
    assert not overlay.property('showing'), 'Overlay watchdog failed'
    assert not warnings, warnings
    print('PASS: startup priming, four 200ms phases, open-blade star tail, early return, same-instance reuse, watchdog; Qt 6.4.1 desktop only.')
    window.close()


if __name__ == '__main__':
    main()
