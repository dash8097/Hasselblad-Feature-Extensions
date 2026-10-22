/* 像素超频参数页，复用原厂行组件和参数选择器；固定六合一及完成后保持。 */
import QtQuick
import com.hasselblad.constants
import com.hasselblad.keys
import com.hasselblad.types
import "qrc:/app/qml/components"
import "qrc:/app/qml/mainmenu"
import "qrc:/app/qml/scripts/Keys.js" as MKeys

FocusScope {
    id: root
    objectName: "X2dMultishotSettings_root"
    property var controller: null
    property var viewModel: null
    property var exposureModel: null
    property bool externalMenu: true
    // 参数只在应用模式前可改；光圈、快门、ISO 显示全局设置。
    property bool stackSettings: false
    property int stackMethod: 2
    property bool averageNoiseReduction: false
    property int stackTotalSeconds: 60
    property int stackInitialDelay: 2
    property int stackTargetIso: 100
    // 实际帧数和间隙来自当前任务，不按曝光时间推算。
    property int stackFrameCount: -1
    property real stackGapSeconds: -1
    property bool editingTotalTime: false
    readonly property var stackMethodLabels: ["最大值", "最小值", "平均值", "ISO 堆栈"]
    readonly property bool isoStack: stackSettings && stackMethod === 3
    readonly property int sourceIso: {
        var text=exposureModel && exposureModel.isoAndAutoAsString ? String(exposureModel.isoAndAutoAsString).trim() : ""
        return /^[1-9][0-9]{0,4}$/.test(text) && Number(text)<=65535 ? Number(text) : 0
    }
    readonly property int plannedFrames: controller && (controller.busy || controller.applied) && controller.stackIsoMode && controller.stackTargetIso===stackTargetIso && controller.stackPlannedFrames>0 ? controller.stackPlannedFrames : sourceIso>=stackTargetIso ? Math.ceil(sourceIso/stackTargetIso) : 0
    signal stackMethodChosen(int value)
    signal averageNoiseReductionChosen(bool value)
    signal stackTotalSecondsChosen(int value)
    signal stackInitialDelayChosen(int value)
    signal stackTargetIsoChosen(int value)
    property int currentRow: 0
    onEditingTotalTimeChanged: rows.contentY = 0
    onCurrentRowChanged: {
        var top = currentRow * Constants.menuListItemDefaultHeight
        if (top < rows.contentY) rows.contentY = top
        else if (top + Constants.menuListItemDefaultHeight > rows.contentY + rows.height)
            rows.contentY = Math.min(Math.max(0, rows.contentHeight - rows.height), top + Constants.menuListItemDefaultHeight - rows.height)
    }
    readonly property bool editable: controller !== null && !controller.busy
    readonly property bool popupActive: selector.active
    readonly property bool preventSwipe: selector.active
    readonly property int keyOverride: HblmTypes.E_CameraKeyOption_OverrideHalfPress
    signal close()
    signal closeExternal()

    function durationText(seconds) {
        function pad(value) { return value < 10 ? "0" + value : String(value) }
        return pad(Math.floor(seconds / 3600)) + ":" + pad(Math.floor(seconds / 60) % 60) + ":" + pad(seconds % 60)
    }
    function rowEditable(row) {
        return stackSettings ? editingTotalTime || row === 0 || row === 1 || row === 4 || (row === 8 && stackMethod < 2) : row >= 0 && row <= 2 && !(row > 0 && controller && controller.pixelResolution === 25)
    }
    function leavePage(external) {
        if (editingTotalTime && !external) { editingTotalTime = false; currentRow = 1; return }
        if (external) closeExternal(); else close()
    }
    function valueText(row) {
        if (stackSettings) {
            if (editingTotalTime) return String(row === 0 ? Math.floor(stackTotalSeconds / 3600) : row === 1 ? Math.floor(stackTotalSeconds / 60) % 60 : stackTotalSeconds % 60)
            if (row === 0) return stackMethodLabels[stackMethod]
            if (row === 1) return isoStack ? String(stackTargetIso) : durationText(stackTotalSeconds)
            if (row === 2) return isoStack ? (stackFrameCount>=0 ? stackFrameCount : 0) + "／" + (plannedFrames>0 ? plannedFrames : "—") + " 帧" : stackFrameCount >= 0 ? stackFrameCount + " 帧" : "—"
            if (row === 3) return stackFrameCount >= 2 && stackGapSeconds >= 0 ? stackGapSeconds.toFixed(2) + " 秒" : "—"
            if (row === 4) return stackInitialDelay === 0 ? "关闭" : stackInitialDelay + " 秒"
            if (row === 5) return exposureModel && exposureModel.aperture ? exposureModel.aperture : "—"
            if (row === 6) return exposureModel && exposureModel.shutterSpeed ? exposureModel.shutterSpeed : "—"
            if (row === 7) return exposureModel && exposureModel.isoAndAutoAsString ? exposureModel.isoAndAutoAsString : "—"
            if(isoStack && row === 8) return controller && controller.stackOutputIso>0 ? controller.stackOutputIso.toFixed(2) : "—"
            if(isoStack && row === 9) return controller && controller.stackOutputSeconds>0 ? controller.stackOutputShutterText : "—"
            return averageNoiseReduction ? "开启" : "关闭"
        }
        if (!controller) return "—"
        if (row === 0) return controller.pixelResolution === 25 ? "25MP" : controller.pixelResolution === 16 ? "十六亿" : "四亿"
        if (row === 1) return controller.pixelResolution === 25 ? "关闭" : controller.initialDelay + " 秒"
        return controller.pixelResolution === 25 || controller.keepMode ? "保持" : "退出"
    }
    function openRow(row) {
        if (!editable || !rowEditable(row)) return false
        if (stackSettings && !isoStack && !editingTotalTime && row === 1) { editingTotalTime = true; currentRow = 0; return true }
        currentRow = row
        selector.active = true
        return true
    }
    function closePopup() { selector.active = false }
    function choose(key, value) {
        if (!editable) return false
        if (stackSettings) {
            if (key === "stackInitialDelay" && Number.isInteger(value) && value >= 0 && value <= 60) {
                stackInitialDelay = value; stackInitialDelayChosen(value); return true
            }
            if (key === "stackTargetIso" && Number.isInteger(value) && value>=1 && value<=65535) {
                stackTargetIso=value;stackTargetIsoChosen(value);return true
            }
            if (key === "stackMethod" && (value === 0 || value === 1 || value === 2 || value === 3)) {
                if(value===3 && sourceIso>0 && stackTargetIso>sourceIso){stackTargetIso=sourceIso;stackTargetIsoChosen(sourceIso)}
                stackMethod = value; stackMethodChosen(value); currentRow = 0; return true
            }
            if (key === "averageNoiseReduction" && stackMethod < 2 && (value === 0 || value === 1)) {
                averageNoiseReduction = value === 1; averageNoiseReductionChosen(averageNoiseReduction); return true
            }
            var parts = [Math.floor(stackTotalSeconds / 3600), Math.floor(stackTotalSeconds / 60) % 60, stackTotalSeconds % 60]
            var part = ["durationHours", "durationMinutes", "durationSeconds"].indexOf(key)
            if (editingTotalTime && part >= 0 && Number.isInteger(value) && value >= 0 && value <= (part === 0 ? 23 : 59)) {
                parts[part] = value
                var seconds = parts[0] * 3600 + parts[1] * 60 + parts[2]
                if (seconds < 1) return false
                stackTotalSeconds = seconds; stackTotalSecondsChosen(seconds); return true
            }
            return false
        }
        return controller.setOption(key, value)
    }

    Rectangle { anchors.fill: parent; color: Constants.menuBackgroundColor }
    MenuHeader {
        id: header
        height: defaultHeight
        anchors { top: parent.top; left: parent.left; right: parent.right }
        externalMenu: root.externalMenu
        text: [{ context: "", text: root.stackSettings ? (root.editingTotalTime ? "总时间" : "RAW 堆栈") : "像素超频", color: Constants.settingsMenuHeaderSuffixFontColor }]
        onClose: root.leavePage(false)
    }
    Flickable {
        id: rows
        objectName: "X2dMultishotSettings_rows"
        anchors { top: header.bottom; bottom: parent.bottom; left: parent.left; right: parent.right }
        clip: true
        contentHeight: content.height
        boundsBehavior: Flickable.StopAtBounds
        Column {
        id: content
        width: rows.width
        Repeater {
            model: root.stackSettings ? (root.editingTotalTime ? ["小时", "分钟", "秒"] : root.isoStack ? ["堆栈方式", "目标 ISO", "帧数", "间隙时间", "初始延迟", "光圈", "拍摄快门", "拍摄 ISO", "成片 ISO", "成片快门"] : root.stackMethod === 2 ? ["堆栈方式", "总时间", "帧数", "间隙时间", "初始延迟", "光圈", "快门", "ISO"] : ["堆栈方式", "总时间", "帧数", "间隙时间", "初始延迟", "光圈", "快门", "ISO", "平均值降噪"]) : ["像素数量", "初始延迟", "拍摄结束后"]
            delegate: Item {
                objectName: "X2dMultishotSettings_row" + index
                required property int index
                required property string modelData
                width: root.width
                height: Constants.menuListItemDefaultHeight
                Rectangle {
                    anchors.fill: parent
                    color: Constants.highlightColor
                    visible: root.activeFocus && root.currentRow === index
                    opacity: 0.25
                }
                TextValueRow {
                    objectName: "X2dMultishotSettings_value" + index
                    anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter
                        leftMargin: Constants.settingsMenuSettingLeftMargin
                        rightMargin: Constants.settingsMenuSettingRightMargin }
                    text: modelData; valueText: root.valueText(index)
                    pixelSize: Constants.settingsMenuSettingFontSize
                    rightText.font.pixelSize: Constants.settingsMenuTextFontSize
                    rightText.color: root.stackSettings || root.rowEditable(index) ? Constants.colorWhite : Constants.popupTextDisabledColor
                    enabled: root.stackSettings || root.editable
                }
                MouseArea { anchors.fill: parent; enabled: root.editable && root.rowEditable(index); onClicked: root.openRow(index) }
            }
        }
        }
    }
    Keys.onPressed: (event)=> {
        if (MKeys.pressedFnListAcc([KeyFn.Abort, KeyFn.JstkEscape], event)) root.leavePage(false)
        else if (MKeys.pressedFnAcc(KeyFn.HalfPress, event)) root.leavePage(true)
        else if (MKeys.pressedFnListAcc([KeyFn.ListUp, KeyFn.JstkUp], event)) currentRow = root.stackSettings ? (editingTotalTime ? Math.max(0, currentRow-1) : currentRow >= 8 ? 4 : currentRow >= 4 ? 1 : 0) : Math.max(0,currentRow-1)
        else if (MKeys.pressedFnListAcc([KeyFn.ListDown, KeyFn.JstkDown], event)) currentRow = root.stackSettings ? (editingTotalTime ? Math.min(2, currentRow + 1) : currentRow === 0 ? 1 : currentRow === 1 ? 4 : stackMethod < 2 ? 8 : 4) : Math.min(2,currentRow+1)
        else if (MKeys.pressedFnListAcc([KeyFn.Select, KeyFn.PopAccept], event)) openRow(currentRow)
    }
    Loader {
        id: selector
        anchors.fill: parent
        active: false
        source: "X2dMultishotSelector.qml"
        onLoaded: {
            if (root.stackSettings) {
                item.optionKey = root.editingTotalTime ? ["durationHours", "durationMinutes", "durationSeconds"][currentRow] : currentRow === 0 ? "stackMethod" : currentRow === 1 && root.isoStack ? "stackTargetIso" : currentRow === 4 ? "stackInitialDelay" : "averageNoiseReduction"
                item.externalMenu = root.externalMenu
                item.interactive = !root.viewModel || !root.viewModel.screenEVF
                var choices = [], names = []
                if (root.editingTotalTime) for (var n = 0; n <= (currentRow === 0 ? 23 : 59); ++n) { choices.push(n); names.push(String(n)) }
                if (!root.editingTotalTime && currentRow === 4) for (var d = 0; d <= 60; ++d) { choices.push(d); names.push(d === 0 ? "关闭" : d + " 秒") }
                if(root.isoStack && currentRow===1){
                    var isoChoices=[1,2,4,8,16,25,32,50,64,100,125,160,200,250,320,400,500,640,800,1000,1250,1600,2000,2500,3200,4000,5000,6400,8000,10000,12800,16000,20000,25600,51200]
                    if(isoChoices.indexOf(root.stackTargetIso)<0){isoChoices.push(root.stackTargetIso);isoChoices.sort(function(a,b){return a-b})}
                    for(var i=0;i<isoChoices.length;i++)if(!root.sourceIso || isoChoices[i]<=root.sourceIso){choices.push(isoChoices[i]);names.push(String(isoChoices[i]))}
                }
                var numeric=root.editingTotalTime || currentRow===4 || (root.isoStack && currentRow===1)
                item.values = numeric ? choices : currentRow === 0 ? [0, 1, 2, 3] : [0, 1]
                item.labels = numeric ? names : currentRow === 0 ? root.stackMethodLabels : ["关闭", "开启"]
                item.propValue = root.editingTotalTime ? Number(root.valueText(currentRow)) : currentRow === 0 ? root.stackMethod : root.isoStack && currentRow===1 ? root.stackTargetIso : currentRow === 4 ? root.stackInitialDelay : root.averageNoiseReduction ? 1 : 0
                item.setup(); item.forceActiveFocus(); return
            }
            var keys = ["pixelResolution", "initialDelay", "keepMode"]
            var values = currentRow === 0 ? [25, 6, 16] : currentRow === 1 ? [] : [0, 1]
            var labels = currentRow === 0 ? ["25MP", "四亿", "十六亿"] : currentRow === 2 ? ["退出", "保持"] : []
            if (currentRow === 1) for (var seconds = 2; seconds <= 60; ++seconds) {
                values.push(seconds); labels.push(seconds + " 秒")
            }
            item.optionKey = keys[currentRow]
            item.externalMenu = root.externalMenu
            item.interactive = !root.viewModel || !root.viewModel.screenEVF
            item.values = values; item.labels = labels
            item.propValue = currentRow === 0 ? controller.pixelResolution : currentRow === 1 ? controller.initialDelay : controller.keepMode ? 1 : 0
            item.setup(); item.forceActiveFocus()
        }
    }
    Connections {
        target: selector.item
        function onOptionChosen(key, value) { root.choose(key, value) }
        function onClose() { root.closePopup(); root.forceActiveFocus() }
        function onCloseExternal() { root.closePopup(); root.closeExternal() }
    }
}
