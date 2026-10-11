"""桌面检查参数控件的颜色、文字布局和键盘信号。需 PySide6。"""
import os
os.environ.setdefault('QT_QPA_PLATFORM','offscreen')
os.environ.setdefault('QT_QUICK_BACKEND','software')
os.environ['QML_DISABLE_DISK_CACHE']='1'
from pathlib import Path
from PySide6.QtCore import QUrl, Qt
from PySide6.QtGui import QGuiApplication
from PySide6.QtQml import QQmlExpression, QQmlEngine
from PySide6.QtQuick import QQuickView
from PySide6.QtTest import QTest

app=QGuiApplication([])
view=QQuickView()
warnings=[]
view.engine().warnings.connect(lambda items:warnings.extend(e.toString() for e in items))
view.setSource(QUrl.fromLocalFile(str(Path(__file__).resolve().parents[1]/'x1d/wifi-region/temporary-ui/ParameterChoiceTile.qml')))
assert view.status()==QQuickView.Ready,[e.toString() for e in view.errors()]
root=view.rootObject()
root.setProperty('text','参数')
root.setProperty('highlightColor','#345678')
root.setProperty('fitText',True)
view.show();root.forceActiveFocus();QTest.qWait(50)
expr=QQmlExpression(QQmlEngine.contextForObject(root),root,'children[0].color.toString()')
value=expr.evaluate()
if isinstance(value,tuple):value=value[0]
assert not expr.hasError() and value=='#345678',value
clicks=[]
root.clicked.connect(lambda:clicks.append(True))
QTest.keyClick(view,Qt.Key_Return)
assert len(clicks)==1
root.setProperty('acceptKeys',False)
QTest.keyClick(view,Qt.Key_Return)
assert len(clicks)==1
assert not warnings,warnings
view.close()
print('PASS: caller color, text fitting, keyboard signal and disabled-key guard')
