"""验证电子快门提示条件；不连接相机，不触发拍摄或无线发送。"""
from pathlib import Path
import os, sys, json
HERE = Path(__file__).resolve().parent.parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(ROOT / 'x1d/wireless-flash/build/ui-test-python'))
os.environ['QT_QPA_PLATFORM'] = 'offscreen'
from PySide6.QtCore import QUrl
from PySide6.QtGui import QGuiApplication
from PySide6.QtQuick import QQuickView

app = QGuiApplication([])
view = QQuickView()
view.setSource(QUrl.fromLocalFile(str(HERE / 'ElectronicFlashHint.qml')))
assert view.status() == QQuickView.Ready, [e.toString() for e in view.errors()]
view.show()
app.processEvents()
hint = view.rootObject()
checks = 0
for enabled in (False, True):
    for electronic in (False, True):
        for manual in (False, True):
            for valid in (False, True):
                for exposure in (0, 200000, 249999, 250000, 333333, 500000, 500001, 1000000):
                    for display in (False, True):
                        props = dict(flashEnabled=enabled, electronic=electronic,
                                     manualExposure=manual, exposureValid=valid,
                                     exposureUs=exposure, displayAllowed=display)
                        for name, value in props.items():
                            hint.setProperty(name, value)
                        expected = display and enabled and electronic and (not manual or not (valid and 250000 <= exposure <= 500000))
                        assert hint.property('visible') == expected, props
                        checks += 1
assert not hint.property('activeFocus')
print(json.dumps(dict(passed=True, checks=checks, hardwareRequests=0)))
