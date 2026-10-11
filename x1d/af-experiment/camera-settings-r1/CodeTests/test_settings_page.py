"""Qt 6 桌面离线页面/交互检查；不替代相机 Qt 5.5 验收。"""
import hashlib, json, os, sys
from pathlib import Path
sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parents[1]
ROOT = HERE.parents[2]
assert Path.cwd().resolve() == ROOT
sys.path.insert(0, str(ROOT / 'x1d/wireless-flash/build/ui-test-python'))
os.environ['QT_QPA_PLATFORM'] = 'offscreen'
os.environ['QT_QUICK_BACKEND'] = 'software'
from PySide6.QtCore import QPoint, QUrl, Qt, qVersion
from PySide6.QtGui import QFont, QFontDatabase, QGuiApplication
from PySide6.QtQml import QQmlPropertyMap
from PySide6.QtQuick import QQuickView
from PySide6.QtTest import QTest

OUT = HERE / 'build/page-tests'
OUT.mkdir(exist_ok=True)
app = QGuiApplication([])
QFontDatabase.addApplicationFont('C:/Windows/Fonts/msyh.ttc')
app.setFont(QFont('Microsoft YaHei'))
backend = QQmlPropertyMap()
state = dict(command='', connected=False, busy=False, fastAdvanceAvailable=False, fineAdvanceAvailable=False,
    statusText='AF 配置接口未就绪', revision=0, activeRevision=0, generation=0,
    probe=0, fast=0, fine=0, newDirection=False, fastAdvanceMs=65534, fineAdvanceMs=65534, fastPresets=[65535]*4, finePresets=[65535]*5,
    activeProbe=0, activeFast=0, activeFine=0, activeNewDirection=False, activeFastAdvanceMs=65535, activeFineAdvanceMs=65535)
for key, value in state.items():
    backend.insert(key, value)
commands = []
backend.valueChanged.connect(lambda key, value: commands.append(json.loads(value)) if key == 'command' else None)
view = QQuickView()
warnings = []
view.engine().warnings.connect(lambda items: warnings.extend(str(x.toString()) for x in items))
view.rootContext().setContextProperty('hblAf', backend)
view.setResizeMode(QQuickView.SizeRootObjectToView)
view.setSource(QUrl.fromLocalFile(str(HERE / 'SettingsPage.qml')))
assert view.status() == QQuickView.Ready
view.resize(640, 480)
view.show()
page = view.rootObject()
page.setProperty('pageActive', True)
QTest.qWait(250)
assert commands[-1] == {'op': 'visible', 'value': True}
assert view.grabWindow().save(str(OUT / 'unavailable.png'))

for key, value in dict(connected=True, revision=1, activeRevision=1,
        activeProbe=0, activeFast=0, activeFine=0, statusText='已读取本轮配置').items():
    backend.insert(key, value)
QTest.qWait(250)
assert page.property('loadedRevision') == 1 and not page.property('dirty')
assert view.grabWindow().save(str(OUT / 'confirmed.png'))
QTest.mouseClick(view, Qt.LeftButton, Qt.NoModifier, QPoint(572, 111))
QTest.qWait(20)
assert page.property('dirty')
before = len(commands)
QTest.mouseClick(view, Qt.LeftButton, Qt.NoModifier, QPoint(520, 440))
QTest.qWait(20)
assert len(commands) == before + 1
assert commands[-1] == dict(op='apply', probe=5000, fast=0, fine=0,
                           newDirection=False, fastAdvanceMs=65534, fineAdvanceMs=65534)
# 没有模拟确认之前仍保留 dirty；不能把发送等同应用。
assert page.property('dirty')
backend.insert('busy', True)
before = len(commands)
QTest.mouseClick(view, Qt.LeftButton, Qt.NoModifier, QPoint(520, 440))
assert len(commands) == before
backend.insert('probe', 5000)
backend.insert('revision', 2)
backend.insert('busy', False)
backend.insert('statusText', '已确认保存，下轮 AF 生效')
QTest.qWait(250)
assert not page.property('dirty')
assert backend.value('activeProbe') == 0
assert view.grabWindow().save(str(OUT / 'pending-next-cycle.png'))
# 原厂速度可独立覆盖补偿，首次0是手动绝对零，不是假造原厂毫秒。
QTest.mouseClick(view, Qt.LeftButton, Qt.NoModifier, QPoint(573, 210))
QTest.mouseClick(view, Qt.LeftButton, Qt.NoModifier, QPoint(573, 306))
QTest.mouseClick(view, Qt.LeftButton, Qt.NoModifier, QPoint(573, 306))
QTest.mouseClick(view, Qt.LeftButton, Qt.NoModifier, QPoint(520, 440))
assert commands[-1]['fast'] == 0 and commands[-1]['fine'] == 0
assert commands[-1]['fastAdvanceMs'] == 0 and commands[-1]['fineAdvanceMs'] == 5
QTest.qWait(20)
assert view.grabWindow().save(str(OUT / 'factory-speed-manual-advances.png'))
QTest.mouseClick(view, Qt.LeftButton, Qt.NoModifier, QPoint(270, 181))
QTest.mouseClick(view, Qt.LeftButton, Qt.NoModifier, QPoint(270, 277))
QTest.mouseClick(view, Qt.LeftButton, Qt.NoModifier, QPoint(520, 440))
assert commands[-1]['fastAdvanceMs'] == 65534 and commands[-1]['fineAdvanceMs'] == 65534
# 明确进入手动档，不猜测原厂实际幅值；选择快扫20000、精扫5000。
for _ in range(4):QTest.mouseClick(view, Qt.LeftButton, Qt.NoModifier, QPoint(573, 164))
for _ in range(3):QTest.mouseClick(view, Qt.LeftButton, Qt.NoModifier, QPoint(573, 262))
# 两项可手动预设但未接通执行；切档再回来必须重置为档位预设。
QTest.mouseClick(view, Qt.LeftButton, Qt.NoModifier, QPoint(573, 210))
QTest.mouseClick(view, Qt.LeftButton, Qt.NoModifier, QPoint(573, 210))
QTest.mouseClick(view, Qt.LeftButton, Qt.NoModifier, QPoint(573, 306))
QTest.mouseClick(view, Qt.LeftButton, Qt.NoModifier, QPoint(573, 306))
QTest.mouseClick(view, Qt.LeftButton, Qt.NoModifier, QPoint(573, 306))
QTest.qWait(20)
assert view.grabWindow().save(str(OUT / 'independent-advances.png'))
QTest.mouseClick(view, Qt.LeftButton, Qt.NoModifier, QPoint(520, 440))
assert commands[-1]['fastAdvanceMs'] == 5 and commands[-1]['fineAdvanceMs'] == 10
QTest.mouseClick(view, Qt.LeftButton, Qt.NoModifier, QPoint(363, 164))
QTest.mouseClick(view, Qt.LeftButton, Qt.NoModifier, QPoint(573, 164))
QTest.mouseClick(view, Qt.LeftButton, Qt.NoModifier, QPoint(520, 440))
assert commands[-1]['fastAdvanceMs'] == 65535 and commands[-1]['fineAdvanceMs'] == 10
returned = []
page.backRequested.connect(lambda: returned.append(True))
QTest.mouseClick(view, Qt.LeftButton, Qt.NoModifier, QPoint(584, 34))
assert returned == [True]
page.setProperty('pageActive', False)
assert commands[-1] == dict(op='visible', value=False)
view.close()
assert not warnings, warnings
report = {'passed': True, 'hardwareRequests': 0, 'qt': qVersion(),
    'qmlSha256': hashlib.sha256((HERE / 'SettingsPage.qml').read_bytes()).hexdigest(),
    'checks': ['页面可加载', '可见生命周期', '默认跟随原厂与手动档分开', '独立阶段修改', '未确认保持草稿', '等待期间防重复保存',
               '确认后仍区分本轮配置', '原厂速度与人工提前量独立', '恢复原厂撤销人工覆盖', '两项绝对提前量独立保存', '切档恢复预设，不记忆手调值', '返回信号'],
    'limits': ['桌面 Qt 6 / 软件渲染；不是目标 Qt 5.5.1 的真实 IPC 或机内显示验收']}
(OUT / 'validation.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print(json.dumps(report, ensure_ascii=False))
