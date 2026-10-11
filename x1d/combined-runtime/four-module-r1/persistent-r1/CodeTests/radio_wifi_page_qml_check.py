from pathlib import Path
import os,sys,json
sys.dont_write_bytecode=True
P=Path(__file__).resolve().parents[1];ROOT=P.parents[3]
sys.path.insert(0,str(ROOT/'x1d/wireless-flash/build/ui-test-python'))
os.environ['QT_QPA_PLATFORM']='offscreen'
from PySide6.QtCore import QUrl, QPoint, Qt
from PySide6.QtGui import QGuiApplication
from PySide6.QtQml import QQmlPropertyMap
from PySide6.QtQuick import QQuickView
from PySide6.QtTest import QTest
app=QGuiApplication([])
adapter=QQmlPropertyMap()
for k,v in dict(radioMode=0,radioModeBusy=False,radioModeVerified=True,command='').items():adapter.insert(k,v)
view=QQuickView();view.rootContext().setContextProperty('hblNative',adapter)
view.setSource(QUrl.fromLocalFile(str(P/'qml/common/RadioModeSettingsRow.qml')))
assert view.status()==QQuickView.Ready,[e.toString() for e in view.errors()]
view.rootObject().setWidth(500);view.rootObject().setHeight(90);view.resize(500,90);view.show();QTest.qWait(30)
for mode in range(3):
    adapter.insert('radioMode',mode);adapter.insert('command','');QTest.qWait(10)
    QTest.mouseClick(view,Qt.LeftButton,Qt.NoModifier,QPoint(295,45));QTest.qWait(10)
    assert json.loads(adapter.value('command'))==dict(op='radioMode',mode=(mode+1)%3)
for key in ['radioModeBusy','radioModeVerified']:
    adapter.insert(key,key=='radioModeBusy');adapter.insert('command','');QTest.qWait(10)
    QTest.mouseClick(view,Qt.LeftButton,Qt.NoModifier,QPoint(295,45));assert adapter.value('command')==''
    adapter.insert(key,key!='radioModeBusy')
proof=dict(passed=True,cycles=3,busyAndUnverifiedBlocked=True,hardwareRequests=0)
(P/'build/radio-wifi-page/qml-validation.json').write_text(json.dumps(proof,indent=2))
print(json.dumps(proof))
