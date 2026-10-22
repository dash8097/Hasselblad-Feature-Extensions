"""生成取景超时界面候选；不连接相机、不改冻结安装基线。"""
from pathlib import Path
import json, hashlib, re

D = Path(__file__).resolve().parent
X = D.parents[1]
OUT = X / 'outputs/4.2.0/display-controls/liveview-r1'
BASE = X / 'outputs/4.2.0/display-controls/timeout-revision'

def build():
    OUT.mkdir(parents=True, exist_ok=True)
    if (OUT / 'device-install.json').exists():
        raise SystemExit('已装机候选冻结，请使用新目录')
    for p in D.glob('X2dScreenLiveview*.qml'):
        (OUT / p.name).write_bytes(p.read_bytes())
    (OUT / 'X2dScreenParameters.qml').write_bytes((D / 'X2dScreenParameters.qml').read_bytes())
    s = (BASE / 'X2dScreenTimeoutDelegate.qml').read_text('utf-8')
    s = s.replace('import QtQuick\n', 'import QtQuick\nimport QtQuick.Window\n')
    s = s.replace('    readonly property bool hasDescription:', '''    property var timeoutHost: null
    function findTimeout(node) {
        if (!node) return null
        if (node.objectName === "X2dScreenLiveviewTimeout") return node
        var children = node.children || []
        for (var i = 0; i < children.length; ++i) { var hit = findTimeout(children[i]); if (hit) return hit }
        return null
    }
    Component.onCompleted: timeoutHost = findTimeout(root.Window.window ? root.Window.window.contentItem : null)
    readonly property bool hasDescription:''')
    s = s.replace('if (!root.model.itemEnabled)', 'if (!root.model.itemEnabled || !timeoutHost || !timeoutHost.preferences.ready)')
    s = s.replace('X2dScreenTimeoutSelector.qml', 'X2dScreenLiveviewSelector.qml')
    s = s.replace('"propName": root.model.itemName, "propValue": root.model.propValue}',
                  '"propName": root.model.itemName, "propValue": root.model.propValue, "timeoutHost": timeoutHost}')
    s = s.replace('(System.screen_idle_timeout === 0 ? "关闭" : ValueFormatter.getDisplayValue(root.model.itemName, root.model.propValue))',
                  '(timeoutHost ? timeoutHost.valueLabel(root.model.propValue) : "—")')
    # 保存期间秒数会暂时清零以停用计时，必须等持久化回读完成才能关闭。
    start = s.index('    Connections {\n        target: root.haveDropdownOpen ? root.model : null')
    s = s[:start] + '}\n'
    (OUT / 'X2dScreenLiveviewDelegate.qml').write_text(s, encoding='utf-8')

    s = (BASE / 'X2dDisplayModeList.qml').read_text('utf-8')
    s = s.replace('    property var proxy:', '''    property var timeoutHost: null
    property var viewModel: null
    readonly property bool preventSwipe: true
    readonly property var preferences: timeoutHost ? timeoutHost.preferences : null
    property bool pending: false
    property bool leaveExternal: false
    property bool saveFailed: false
    Connections {
        target: root.preferences
        function onSaved() {
            if (!root.pending) return
            root.pending = false; root.valueSaved()
            if (root.leaveExternal) root.closeExternal(); else root.close()
        }
        function onFailedChanged() {
            if (root.preferences.failed) { root.pending = false; root.saveFailed = true }
        }
    }
    property var proxy:''')
    a = s.index('    function setup()')
    b = s.index('    Keys.onPressed:', a)
    s = s[:a] + '''    function setup() {
        if (!preferences || !preferences.ready) { close(); return }
        list.currentIndex = preferences.values.indexOf(preferences.seconds)
        list.visible = true
    }
    function itemSelected(ix) {
        if (pending) return
        list.currentIndex = ix; setSelected()
    }
    function setSelected() {
        if (pending || !preferences || !preferences.ready || list.currentIndex < 0) return
        pending = true; saveFailed = false
        if (!preferences.select(preferences.values[list.currentIndex])) pending = false
    }
''' + s[b:]
    a = s.index('        if (MKeys.pressedFnAcc(KeyFn.HalfPress, event))')
    b = s.index('        } else if (MKeys.pressedFnListAcc', a)
    s = s[:a] + '''        if (MKeys.pressedFnAcc(KeyFn.HalfPress, event)) {
            root.leaveExternal = root.externalMenu; setSelected()
        } else if (MKeys.pressedFnAcc(KeyFn.PopAccept, event)) {
            setSelected()
''' + s[b:]
    s = s.replace('            root.close()\n        } else if (MKeys.pressedFnAcc(KeyFn.NavKey',
                  '            if (!pending) root.close()\n        } else if (MKeys.pressedFnAcc(KeyFn.NavKey')
    s = s.replace('model: root.propName ? ValueFormatter.getVariableListModel(root.propName) : 0',
                  'model: root.preferences ? root.preferences.values : []')
    s = re.sub(r'text: \(\(modelData === .*?\n',
               'text: root.timeoutHost ? root.timeoutHost.valueLabel(modelData) : "—"\n', s)
    assert 'ValueFormatter.stringTranslated' not in s
    s = s.replace('text: modelData\n', 'text: textItem.text\n')
    s = s.replace('            interactive: root.interactive', '            interactive: root.interactive && !root.pending')
    s = s.replace('                    onClicked: root.itemSelected(index)', '                    enabled: !root.pending\n                    onClicked: root.itemSelected(index)')
    # 错误由当前系统语言给出，不把测试接口名暴露给用户。
    s = s.replace('    content: FocusScope {', '''    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        text: root.saveFailed && root.timeoutHost ? root.timeoutHost.errorLabel : ""
        color: "white"; font.pixelSize: 28 * Constants.scaleFactor
    }
    content: FocusScope {''')
    (OUT / 'X2dScreenLiveviewSelector.qml').write_text(s, encoding='utf-8')
    files = [{'name': p.name, 'sha256': hashlib.sha256(p.read_bytes()).hexdigest()}
             for p in sorted(OUT.glob('*.qml'))]
    (OUT / 'candidate.json').write_text(json.dumps({'firmware':'4.2.0', 'deviceValidated':False,
        'files':files, 'defaultSeconds':0, 'configuration':'/blackbox/x2d-liveview-timeout.txt'}, indent=2)+'\n', encoding='utf-8')
    print('Prepared', len(files), 'liveview UI files; no device access')

if __name__ == '__main__':
    build()
