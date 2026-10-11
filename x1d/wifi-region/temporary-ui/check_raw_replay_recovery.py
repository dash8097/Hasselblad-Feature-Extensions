"""Synthetic readiness/retry races; never accesses camera photos."""
from pathlib import Path
import os,sys
P=Path(__file__).resolve().parent
os.environ['QT_QPA_PLATFORM']='offscreen'
sys.path.insert(0,str(P.parents[1]/'wireless-flash/build/ui-test-python'))
from PySide6.QtCore import QUrl
from PySide6.QtGui import QGuiApplication
from PySide6.QtQuick import QQuickView
from PySide6.QtTest import QTest
app=QGuiApplication([]);v=QQuickView()
v.setSource(QUrl.fromLocalFile(str(P/'FullResolutionRetry.qml')))
assert v.status()==QQuickView.Ready,[e.toString() for e in v.errors()]
r=v.rootObject();r.setProperty('baseDelay',10)
calls=[]
def reload():
    calls.append(1)
    r.setProperty('imageStatus',0)
    r.setProperty('imageStatus',3)
r.reloadRequested.connect(reload)
r.setProperty('requestIdentity','session-1-photo-1')
r.setProperty('requestActive',True);r.setProperty('imageStatus',3)
QTest.qWait(50);assert not calls,'wait for storage readiness'
r.setProperty('storageReady',True);QTest.qWait(200)
assert len(calls)==3 and r.property('attempts')==3,'bounded retry despite source clearing'
r.setProperty('requestActive',False);QTest.qWait(50);assert len(calls)==3
r.setProperty('imageStatus',2);r.setProperty('requestIdentity','session-2-photo-1')
r.setProperty('requestActive',True);QTest.qWait(80);assert len(calls)==3,'do not cancel loading'
r.setProperty('imageStatus',1);r.setProperty('expectedWidth',8000);r.setProperty('expectedHeight',6000)
r.setProperty('imageWidth',8000);r.setProperty('imageHeight',6000);QTest.qWait(60)
assert len(calls)==3,'full image needs no retry'
r.setProperty('imageWidth',640);r.setProperty('imageHeight',480);QTest.qWait(25)
assert len(calls)>3,'ready thumbnail is not full resolution'
r.setProperty('requestActive',False);before=len(calls);QTest.qWait(100)
assert len(calls)==before,'leaving cancels retry'
print('PASS: storage-ready race, bounded retries, no loading interruption, resolution check, exit cancellation')
