import QtQuick
import com.hasselblad.constants
import com.hasselblad.types
import com.hasselblad.keys
import "qrc:/app/qml/scripts/Keys.js" as MKeys
import "qrc:/app/qml/exposescreen"

// 原厂曝光屏、倒计时图文、进度条及按钮；不另设字体、字号和布局。
ExposeScreen {
    id: root
    objectName: "PixelOverclockPage"
    property var controller: null
    property string applicationError: ""
    readonly property int keyOverride: visible ? HblmTypes.E_CameraKeyOption_Override : 0
    signal close()
    readonly property bool counting: controller && controller.stage === "delay"
    readonly property bool finishingAllowed: controller && controller.stackMode && controller.busy && controller.stage === "capture"
    headerText: controller && controller.stackMode ? (controller.stackIsoMode ? "ISO 堆栈" : "RAW 堆栈") : "像素超频"
    showButtons: controller && (!controller.busy || finishingAllowed)
    btn2.text: finishingAllowed || (controller && controller.stage === "complete") ? "完成" : "返回"
    btn2.enabled: root.showButtons
    onAbortPressed: { if(finishingAllowed) controller.finishStack(); else if(root.showButtons) root.close() }
    Keys.onPressed: (event)=> {
        // 独立曝光页接原厂叉号/返回映射，完成不依赖按钮绘制的 available 回写。
        if(root.showButtons && MKeys.pressedFnListAcc([KeyFn.Abort,KeyFn.Escape,KeyFn.JstkEscape],event)) {
            if(root.finishingAllowed) controller.finishStack(); else root.close()
        }
        if(!MKeys.isUnblockableCustomButton(event)) event.accepted=true
    }
    // 原厂确认区只绘制实体按键提示；其曝光屏遮罩不处理触摸返回。
    // 复用确认区的实际边界，只给已有的返回提示增加触摸，不改变样式。
    property var nativeButtonArea: null
    function findButtonArea(item) {
        if (!item) return null
        if (item.objectName === "ConfirmButtonArea") return item
        for (var i=0;i<item.children.length;i++) {
            var result=findButtonArea(item.children[i])
            if (result) return result
        }
        return null
    }
    Component.onCompleted: nativeButtonArea=findButtonArea(root)
    MouseArea {
        objectName: "PixelOverclockReturnTouch"
        z: 201
        enabled: root.showButtons && root.nativeButtonArea !== null
        visible: enabled
        x: root.sideOriented && root.nativeButtonArea ? root.width-root.nativeButtonArea.background.width : 0
        y: root.sideOriented ? root.height/2 : root.nativeButtonArea ? root.height-root.nativeButtonArea.background.height : root.height
        width: root.sideOriented && root.nativeButtonArea ? root.nativeButtonArea.background.width : root.width/2
        height: root.height-y
        onClicked: { if(root.finishingAllowed) controller.finishStack(); else if(root.showButtons) root.close() }
    }
    // 原厂曝光屏自身透明，原页面由上层提供黑底；独立 Loader 也必须补上同一黑底。
    Rectangle { objectName:"PixelOverclockNativeBackdrop"; anchors.fill:parent; color:Constants.colorBlack; z:-1 }
    function stageText() {
        if(applicationError && controller && !controller.busy) return "模式应用失败"
        if(controller && controller.preparingMode) return "正在准备"
        if(controller && controller.stage === "failed") return "处理失败"
        if(controller && controller.stackMode && controller.stage === "capture")
            return controller.stackRemaining > 0 ? controller.stackRemaining + " 秒" : "收尾中"
        if (!controller || !controller.ready && !controller.busy) return "后台未就绪"
        var names = {idle:"已就绪", prepare:"正在准备", delay:"等待拍摄", capture:"拍摄 " + controller.captureFrames + " 张",
                     merge:"合成中", render:"生成 JPEG", jpeg:"生成 JPEG", jpeg_join:"生成 JPEG", package:"封装成片", convert:"转换 HEIF", raw:"保存 3FR", publish:"保存成片", restore:"恢复相机",
                     cleanup:"清理中间文件", complete:"成片已保存", failed:"处理失败"}
        if (controller.stage === "render" && !controller.stackMode && controller.pixelResolution === 6 && controller.renderTiles < 9)
            return "显影"
        if (controller.stage === "render" && !controller.stackMode && controller.pixelResolution === 16) return "显影"
        if (controller.stage === "restore" && !controller.stackMode && controller.pixelResolution === 16) return "恢复拍摄设置"
        return names[controller.stage] || "处理中"
    }
    ExposeIconText {
        objectName: "PixelOverclockNativeCountdown"
        anchors.fill: parent
        visible: root.counting
        text1.text: controller && controller.countdownRemaining>0 && root.viewModel ? root.viewModel.tdur(controller.countdownRemaining) : ""
        iconSpacing: 16 * 1.6 * Constants.scaleFactorX
        icon.source: "image://svg/ic_drive_mode_self_timer_big"
    }
    ExposeProgress {
        objectName: "PixelOverclockNativeProgress"
        anchors.fill: parent
        visible: !root.counting
        // 对照 4.2.0 IntervalTimerScreen：状态为 Normal，数量为 DemiBold。
        // 模式名称只放原厂顶部标题；进度条下方只放一行帧间隔。
        readonly property real rowFontSize: text1.text.length + text2.text.length > 12 ? 80 * Constants.scaleFactorX : 60 * 1.6 * Constants.scaleFactor
        rowSpacing: text2.text !== "" ? 40 * 1.6 * Constants.scaleFactorX : 0
        text1.font.pixelSize: rowFontSize
        text2.font.pixelSize: rowFontSize
        text1.text: root.stageText()
        text2.text: {
            if (!controller || controller.preparingMode || controller.stage === "failed") return ""
            if (controller.stackMode && controller.stage === "capture")
                return controller.stackFrameCount + " 帧"
            if (!controller.stackMode && controller.pixelResolution === 16) {
                if (controller.stage === "render") return Math.min(controller.renderTiles + 1, 25) + "/25"
                if (controller.stage === "jpeg") return Math.min(controller.jpegTiles + 1, 25) + "/25"
                if (controller.stage === "jpeg_join")
                    return controller.jpegJoinComplete ? "100%" : controller.jpegJoinTotal>0 ?
                        Math.min(99,Math.floor(controller.jpegJoinRows*100/controller.jpegJoinTotal))+"%" : "0%"
            }
            if (!controller.stackMode && controller.pixelResolution === 6 && controller.stage === "render" && controller.renderTiles < 9)
                return controller.renderTiles + "/9"
            return ""
        }
        text3.text: {
            if (!controller) return ""
            if (!controller.busy) return root.applicationError || controller.lastError
            if(controller.stackIsoMode && controller.stage === "capture")
                return (controller.stackPlannedFrames>0 ? "计划 " + controller.stackPlannedFrames + " 帧" : "读取拍摄参数") +
                    "\n成片 ISO " + (controller.stackOutputIso>0 ? controller.stackOutputIso.toFixed(2) : "—") +
                    " · 快门 " + controller.stackOutputShutterText
            if(controller.stackMode && controller.stage === "capture")
                return "帧间隔 " + (controller.stackFrameCount >= 2 ? (controller.stackGapMs/1000).toFixed(2) + " 秒" : "—")
            if (controller.stage === "prepare" || controller.stage === "delay" || controller.stage === "capture")
                return "请保持相机稳定"
            if (controller.stage === "jpeg_join")
                return controller.jpegJoinRows===controller.jpegJoinTotal && controller.jpegJoinTotal>0 && !controller.jpegJoinComplete ? "成片写入中" : "完成 25/25 块编码"
            return controller.workTotal > 0 ? "已完成 " + controller.workDone + "/" + controller.workTotal + " 步" : "采集已结束"
        }
        // 原厂白色填充条显示实际完成的工作单元；不使用无法核验的时长百分比。
        showInfiniteLoop: controller && controller.stage === "jpeg_join" && controller.jpegJoinTotal === 0
        progressValue: controller && controller.stage === "complete" ? 1 :
            controller && controller.stage === "jpeg_join" && controller.jpegJoinTotal>0 ?
                (controller.jpegJoinComplete ? 1 : Math.min(0.99,controller.jpegJoinRows/controller.jpegJoinTotal)) :
            controller && controller.workTotal > 0 ? controller.workDone / controller.workTotal : 0
    }
}
