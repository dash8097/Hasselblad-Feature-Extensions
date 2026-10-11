"""真实 QML 交互：五档编辑、取消、边界和忙时拒绝；无线对象为替身。"""
from pathlib import Path
import os,sys,json
sys.dont_write_bytecode=True
os.environ['QT_QPA_PLATFORM']='offscreen';os.environ['QT_QUICK_BACKEND']='software'
HERE=Path(__file__).resolve().parent;WORK=HERE.parent;ROOT=WORK.parents[3]
sys.path.insert(0,str(ROOT/'x1d/wireless-flash/build/ui-test-python'))
from PySide6.QtCore import QUrl,QObject,QPoint,Qt
from PySide6.QtGui import QGuiApplication,QFontDatabase,QFont
from PySide6.QtQuick import QQuickWindow
from PySide6.QtQml import QQmlApplicationEngine
from PySide6.QtTest import QTest
app=QGuiApplication([])
font_id=QFontDatabase.addApplicationFont('C:/Windows/Fonts/msyh.ttc');assert font_id>=0
family=QFontDatabase.applicationFontFamilies(font_id)[0];app.setFont(QFont(family))
asset=(ROOT/'x1d/candidates/ui-resident/revisions/full-pages-r2/build/host/r2-diagnostic/qml/icons').as_uri()+'/'
qml='''import QtQuick 2.5
import QtQuick.Window 2.2
import "@COMPONENTS@"
Window {
    width:640; height:480; visible:true
    property int changes:0
    FlashPage {
        id:page; anchors.fill:parent; pageActive:true; iconBase:"@ICONS@"; fontName:"@FONT@"
        screen:"settings"
        onMechanicalCalibrationRequested: {
            var copy=mechanicalDelays.slice(0);copy[index]=microseconds;mechanicalDelays=copy
            changes++
        }
    }
}'''.replace('@COMPONENTS@',(WORK/'qml/controlscreen').as_uri()).replace('@ICONS@',asset).replace('@FONT@',family)
errors=[];engine=QQmlApplicationEngine();engine.warnings.connect(lambda items:errors.extend(e.toString() for e in items))
engine.loadData(qml.encode(),QUrl('file:///calibration-ui-check.qml'))
assert engine.rootObjects(),errors
window=engine.rootObjects()[0];QTest.qWait(80)
page=window.findChild(QObject,'FormalFlashPage');dialog=window.findChild(QObject,'FlashCalibrationDialog')
def click(name):
    def find(item):
        if item.objectName()==name:return item
        for child in item.childItems():
            found=find(child)
            if found is not None:return found
        return None
    item=find(window.contentItem());assert item is not None,(name,errors)
    point=item.mapToScene(item.boundingRect().center())
    QTest.mouseClick(window,Qt.LeftButton,Qt.NoModifier,QPoint(round(point.x()),round(point.y())))
    app.processEvents()
out=HERE/'calibration-ui-output';out.mkdir(exist_ok=True)
assert window.grabWindow().save(str(out/'settings.png'))
click('FlashCalibrationOpen');assert page.property('calibrationOpen')
click('FlashDelayPlus0');assert window.property('changes')==1
click('FlashDelayValue2');assert dialog.property('selected')==2
for key in ['7','.','2','5']:click('FlashDelayKey'+key)
assert dialog.property('input')=='7.25'
click('FlashDelayConfirm');assert dialog.property('selected')==-1 and window.property('changes')==2
assert window.grabWindow().save(str(out/'five-points.png'))
click('FlashDelayValue4');click('FlashDelayKey9');click('FlashDelayCancel')
assert window.property('changes')==2
click('FlashDelayValue1');dialog.setProperty('input','5000.01');click('FlashDelayConfirm')
assert dialog.property('selected')==1 and window.property('changes')==2
dialog.setProperty('input','0.01');click('FlashDelayConfirm');assert window.property('changes')==3
page.setProperty('calibrationEditable',False);click('FlashDelayPlus0');assert window.property('changes')==3
page.setProperty('calibrationEditable',True)
click('FlashCalibrationClose');assert not page.property('calibrationOpen')
click('FlashCalibrationOpen');click('FlashDelayValue2');assert dialog.property('input')=='7.25'
assert window.grabWindow().save(str(out/'numeric-input.png'))
assert not errors,errors
proof={'passed':True,'fivePointEditing':True,'numericMilliseconds':True,'cancelPreservesValue':True,'boundsValidated':True,'busyRejectsEdit':True,'qmlWarnings':errors,'hardwareRequests':0}
(out/'validation.json').write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps(proof))
window.close();engine.deleteLater();app.processEvents()
