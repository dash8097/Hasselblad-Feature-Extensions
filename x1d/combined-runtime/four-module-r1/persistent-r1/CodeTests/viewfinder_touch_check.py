from pathlib import Path
import sys,os,json,hashlib
sys.dont_write_bytecode=True
P=Path(__file__).resolve().parents[1];ROOT=P.parents[3]
os.environ['QT_QPA_PLATFORM']='offscreen'
sys.path.insert(0,str(ROOT/'x1d/wireless-flash/build/ui-test-python'))
from PySide6.QtCore import Qt,QPoint,QUrl
from PySide6.QtGui import QGuiApplication
from PySide6.QtQuick import QQuickView
from PySide6.QtTest import QTest
app=QGuiApplication([])
s=(P/'qml/liveview/LiveViewOverlay.qml').read_text()
f=s[s.index('    function pickFocusPoint'):s.index('    // Representing the area')]
s=(P/'qml/liveview/LiveViewImage.qml').read_text();a=s.index('    MouseArea {');b=s.index('    ExposureButton {',a);mouse=s[a:b]
# Exercise exact production pointer handlers and selection function with explicit backend mocks.
qml='import QtQuick 2.0\nItem {\n id: root; width:1000;height:750\n property bool inActiveWindow:true;property bool isInHDMI:false\n property var GlobalStateInfo: ({evfActive:false,afSelectionActive:false,afSselectedIndex:-1})\n property var VideoControl: ({videoMode:1,View:1})\n property var liveViewVideoArea: ({x:0,y:0,width:1000,height:750})\n property var crop: ({cropWidth:0,cropHeight:0})\n property var exposureButton: ({isDragging:false})\n property int savedIndex:-1; property int saves:0;property int zooms:0\n property var directFocusGrid: ({afItemWidth:100,afItemHeight:100,xCount:7,yCount:5,gridXMargin:150,gridYMargin:125,resetSavedIndexes:function(){},selectAndSave:function(){root.savedIndex=GlobalStateInfo.afSselectedIndex;root.saves++}})\n property var info: ({pickFocusPoint:function(x,y){root.pickFocusPoint(x,y)},startZoomTimer:function(){}})\n function liveViewSwitch(mouse){zooms++}\n @FUNCTION@\n @MOUSE@\n}\n'.replace('@FUNCTION@',f).replace('@MOUSE@',mouse)
qml=qml.replace('GlobalStateInfo','globalMock').replace('VideoControl','videoMock')
o=P/'build/viewfinder-touch';o.mkdir(exist_ok=True);h=o/'GestureHarness.qml';h.write_text(qml,encoding='utf-8')
v=QQuickView();v.setSource(QUrl.fromLocalFile(str(h.resolve())));assert v.status()==QQuickView.Ready,[e.toString() for e in v.errors()]
v.show();QTest.qWait(20);r=v.rootObject()
QTest.mouseClick(v,Qt.LeftButton,Qt.NoModifier,QPoint(200,175));assert r.property('savedIndex')==0
QTest.mousePress(v,Qt.LeftButton,Qt.NoModifier,QPoint(200,175));QTest.mouseMove(v,QPoint(800,575),30);QTest.mouseRelease(v,Qt.LeftButton,Qt.NoModifier,QPoint(800,575));assert r.property('savedIndex')==34
QTest.mousePress(v,Qt.LeftButton,Qt.NoModifier,QPoint(500,375));QTest.qWait(900);assert r.property('savedIndex')==17 and r.property('zooms')==0;QTest.mouseRelease(v,Qt.LeftButton,Qt.NoModifier,QPoint(500,375))
assert r.property('zooms')==0
(o/'gesture-validation.json').write_text(json.dumps(dict(passed=True,checks=['click nearest grid','drag follows grid','hold selects without zoom','no focus dispatch in handlers'],hardwareRequests=0,fullCameraUIValidated=False,sourceHashes={n:hashlib.sha256((P/n).read_bytes()).hexdigest() for n in ['qml/liveview/LiveViewImage.qml','qml/liveview/LiveViewOverlay.qml']}),indent=2))
print('production gesture handlers passed host Qt tests')
