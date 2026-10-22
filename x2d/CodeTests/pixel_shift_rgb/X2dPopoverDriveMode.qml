/*
 * vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab fileencoding=utf-8
 * encoding: utf-8
 * -*- coding: utf-8 -*-
 *
 * Copyright 2020 Victor Hasselblad AB.
 *
 * All information contained in or disclosed by this document is confidential
 * and proprietary of Victor Hasselblad AB. By accepting this material the
 * recipient agrees that this material and the information contained therein
 * will be held in confidence and will not be reproduced in whole or in part
 * without written permission from Victor Hasselblad AB.
 */

import QtQuick
import "qrc:/app/qml/popups"

import com.hasselblad.constants
import com.hasselblad.menumodel
import com.hasselblad.types
import com.hasselblad.keys
import com.hasselblad.valueformatter

import "qrc:/app/qml/controlscreen"
import "qrc:/app/qml/components"
import "qrc:/app/qml/mainmenu"
import "qrc:/app/qml/scripts/Keys.js" as MKeys

Popover {
    id: root
    objectName: "PopoverDriveMode_root"

    property MainMenuViewModel mainMenuViewModel: null
    property bool lensHasMicroFocusAdjust: false
    property int expMode: 0
    property int driveMode: 0
    readonly property bool rawStackParametersSelected: right_side.isStack
    readonly property int keyOverride: HblmTypes.E_CameraKeyOption_OverrideHalfPress // Used by KeyFocusHandler.cpp
    readonly property real leftSideWidth: 256 * 1.6 * Constants.scaleFactorX
    readonly property real headerAreaHeight: 56 * 1.6 * Constants.scaleFactorY
    readonly property real headerFontSize: 20 * 1.6 * Constants.scaleFactor
    readonly property int headerFontWeight: Font.Medium

    signal setDriveMode(int mode)
    signal openExtension(string mode)
    signal selectExtension(string mode)
    signal applyExtension(string mode)
    property string selectedExtension: ""
    readonly property bool handlesExitApplication: true
    property bool selectionCommitted: false
    // 先于原厂 ControlScreen 的 closePopup 处理提交；其处理会卸载本弹窗。
    onClosePopup: {
        if (selectionCommitted || !grid.isIndexSelectable(grid.currentIndex)) return
        var choice = grid.model.get(grid.currentIndex)
        if (!choice.extensionMode) return
        selectionCommitted = true
        root.selectExtension(choice.extensionMode)
        root.applyExtension(choice.extensionMode)
    }
    property var multishotController: null
    property var exposureModel: null
    property int stackMethod: 2
    property bool averageNoiseReduction: false
    property int stackTotalSeconds: 60
    property int stackInitialDelay: 2
    property int stackTargetIso: 100
    property int stackPlannedFrames: 0
    property int stackFrameCount: 0
    property real stackGapSeconds: 0
    signal stackMethodChosen(int value)
    signal averageNoiseReductionChosen(bool value)
    signal stackTotalSecondsChosen(int value)
    signal stackInitialDelayChosen(int value)
    signal stackTargetIsoChosen(int value)
    signal setExitDriveMode(int mode)

    contentItem: grid
    headingSize: 0
    contentWidth: 564 * 1.6 * Constants.scaleFactorX
    contentHeight: 426 * 1.6 * Constants.scaleFactorY
    popupTopMargin: 27 * 1.6 * Constants.scaleFactorY

    clickedHandler: popoverClickCloseHandler

    onActiveFocusChanged: {
        if (activeFocus) {
            grid.focus = true
        }
    }

    function popoverClickCloseHandler()
    {
        var closePopover = !menuLoader.popupActive
        if (!closePopover) {
            menuLoader.item.closePopup()
        }

        return closePopover
    }

    function openParameters() {
        if (!right_side.canShowSettings) return
        swipeArea.extensionPage = right_side.isMultishot || right_side.isStack
        swipeArea.settingsList = list.settingsList
        if (swipeArea.extensionPage) {
            menuLoader.active = true
            menuLoader.asynchronous = false
        }
        swipeArea.showLoader(true)
    }
    function closeDriveMode()
    {
        popoverClickCloseHandler()
        root.close()
    }

    content: Item {
        anchors.fill: parent
        clip: true

        Row {
            anchors.fill: parent

            Item {
                id: left_side

                width: root.leftSideWidth
                height: parent.height

                Item {
                    id: left_header_area
                    anchors {
                        top: parent.top
                        left: parent.left
                        right: parent.right
                    }
                    height: root.headerAreaHeight

                    Text {
                        anchors.centerIn: parent
                        text: "拍摄模式"
                        color: Constants.colorWhite
                        font.pixelSize: root.headerFontSize
                        font.weight: root.headerFontWeight
                    }
                }

                X2dDriveModeGrid {
                    id: grid
                    focus: true
                    srcModel: gridModel
                    expMode: root.expMode
                    lensHasMicroFocusAdjust: root.lensHasMicroFocusAdjust
                    driveMode: root.driveMode
                    maxWidth: parent.width
                    anchors {
                        top: parent.top
                        horizontalCenter: parent.horizontalCenter
                        topMargin: 62 * 1.6 * Constants.scaleFactorY
                    }

                    onOpenDialog: (settingsList)=> {
                        swipeArea.extensionPage = false
                        swipeArea.settingsList = settingsList
                        swipeArea.showLoader(true)
                    }
                    selectedExtension: root.selectedExtension
                    onSelectExtension: (mode)=> root.selectExtension(mode)
                    onApplyExtension: (mode)=> { root.selectionCommitted=true; root.applyExtension(mode) }
                    onOpenExtension: (mode)=> root.openParameters()
                    onSetDriveMode: (mode)=> {
                        // Set both drive mode and exit_drive_mode that should be used when exiting from bracketing etc.
                        root.setDriveMode(mode)
                        root.setExitDriveMode(mode)
                    }

                    Component.onCompleted: {
                        close.connect(root.closeDriveMode)
                    }
                }
            }
            Separator {
                anchors {
                    top: parent.top
                    bottom: parent.bottom
                }
                color: Constants.popupBorderColor
                width: Constants.popupBorderWidth
            }
            Item {
                id: right_side
                width: parent.width - left_side.width
                height: parent.height
                readonly property bool isMultishot: grid.currentIndex === 6
                readonly property bool isStack: grid.currentIndex === 7
                readonly property bool canShowSettings: isMultishot || isStack || list.settingsList !== ""

                MouseArea {
                    id: right_side_ma
                    objectName: "DriveModeSummarySettingsTouch"
                    z: 1
                    anchors.fill: parent
                    enabled: right_side.canShowSettings
                    preventStealing: true
                    onClicked: root.openParameters()
                }

                Item {
                    id: right_header_area
                    anchors {
                        top: parent.top
                        left: parent.left
                        right: parent.right
                    }
                    height: root.headerAreaHeight

                    Text {
                        anchors.centerIn: parent
                        text: qsTranslate("DriveModes", list.headerText)
                        color: Constants.colorWhite
                        font.pixelSize: root.headerFontSize
                        font.weight: root.headerFontWeight
                    }
                }

                HblListView {
                    id: list
                    interactive: false
                    spacing: 32 * 1.6 * Constants.scaleFactorY
                    readonly property string headerText: grid.currentIndex >= 0 ? gridModel.get(grid.currentIndex).text : ""
                    readonly property string settingsList: grid.currentIndex >= 0 ? gridModel.get(grid.currentIndex).settingsList : ""
                    readonly property string description: grid.currentIndex >= 0 ? gridModel.get(grid.currentIndex).description : ""
                    readonly property real sideMargins: 14 * 1.6 * Constants.scaleFactorX
                    readonly property real listTopMargin: 67 * 1.6 * Constants.scaleFactorY
                    readonly property int labelFontWeight: Font.Medium
                    readonly property int valueFontWeight: Font.Normal

                    onSettingsListChanged: {
                        if (settingsList === "") {
                            SubMenu.closeMenu(0)
                            list.model = null
                        } else {
                            SubMenu.showMenu(0, settingsList)
                            list.model = SubMenu.getModel(0)
                        }
                    }
                    anchors {
                        fill: parent
                        topMargin: list.listTopMargin
                        leftMargin: list.sideMargins
                        rightMargin: list.sideMargins
                    }

                    Text {
                        visible: list.settingsList === "" && !right_side.isMultishot && !right_side.isStack
                        width: parent.width
                        fontSizeMode: Text.HorizontalFit
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.pixelSize: 25 * 1.6 * Constants.scaleFactor
                        wrapMode: Text.Wrap
                        text: qsTranslate("DriveModes", list.description)
                        color: Constants.colorWhite
                    }

                    Column {
                        visible: right_side.isMultishot
                        width: parent.width
                        spacing: 32 * 1.6 * Constants.scaleFactorY
                        TextValueRow { width: parent.width; text: "像素数量"; valueText: root.multishotController && root.multishotController.pixelResolution === 25 ? "25MP" : root.multishotController && root.multishotController.pixelResolution === 16 ? "十六亿" : "四亿"; rightText.color: Constants.colorWhite; pixelSize: 25 * 1.6 * Constants.scaleFactor }
                        TextValueRow { width: parent.width; text: "初始延迟"; valueText: root.multishotController ? (root.multishotController.pixelResolution === 25 ? "关闭" : root.multishotController.initialDelay + " 秒") : "—"; pixelSize: 25 * 1.6 * Constants.scaleFactor }
                        TextValueRow { width: parent.width; text: "拍摄结束后"; valueText: root.multishotController && root.multishotController.keepMode ? "保持" : "退出"; pixelSize: 25 * 1.6 * Constants.scaleFactor }
                    }
                    Column {
                        id: stackSummary
                        objectName: "RawStackSummary"
                        visible: right_side.isStack
                        width: parent.width
                        readonly property int visibleRowCount: {
                            var count = 0
                            for (var i = 0; i < children.length; ++i) if (children[i].visible) ++count
                            return count
                        }
                        readonly property real visibleRowHeight: {
                            var total = 0
                            for (var i = 0; i < children.length; ++i) if (children[i].visible) total += children[i].height
                            return total
                        }
                        spacing: visibleRowCount > 1 ? Math.max(0, Math.min(list.spacing, (list.height - visibleRowHeight) / (visibleRowCount - 1))) : 0
                        TextValueRow { width: parent.width; text: "堆栈方式"; valueText: ["最大值", "最小值", "平均值", "ISO 堆栈"][root.stackMethod]; pixelSize: 25 * 1.6 * Constants.scaleFactor; labelFontWeight: list.labelFontWeight; valueFontWeight: list.valueFontWeight }
                        TextValueRow { width: parent.width; text: root.stackMethod === 3 ? "目标 ISO" : "总时间"; valueText: root.stackMethod === 3 ? String(root.stackTargetIso) : root.stackTotalSeconds + " 秒"; pixelSize: 25 * 1.6 * Constants.scaleFactor; labelFontWeight: list.labelFontWeight; valueFontWeight: list.valueFontWeight }
                        TextValueRow { width: parent.width; text: "帧数"; valueText: root.stackFrameCount + (root.stackMethod === 3 ? "／" + (root.stackPlannedFrames > 0 ? root.stackPlannedFrames : "—") : "") + " 帧"; pixelSize: 25 * 1.6 * Constants.scaleFactor; labelFontWeight: list.labelFontWeight; valueFontWeight: list.valueFontWeight }
                        TextValueRow { width: parent.width; text: "间隙时间"; valueText: root.stackFrameCount >= 2 ? root.stackGapSeconds.toFixed(2) + " 秒" : "—"; pixelSize: 25 * 1.6 * Constants.scaleFactor; labelFontWeight: list.labelFontWeight; valueFontWeight: list.valueFontWeight }
                        TextValueRow { width: parent.width; text: "初始延迟"; valueText: root.stackInitialDelay === 0 ? "关闭" : root.stackInitialDelay + " 秒"; pixelSize: 25 * 1.6 * Constants.scaleFactor; labelFontWeight: list.labelFontWeight; valueFontWeight: list.valueFontWeight }
                        TextValueRow { visible: root.stackMethod < 2; width: parent.width; text: "平均值降噪"; valueText: root.averageNoiseReduction ? "开启" : "关闭"; pixelSize: 25 * 1.6 * Constants.scaleFactor; labelFontWeight: list.labelFontWeight; valueFontWeight: list.valueFontWeight }
                        Text { width: parent.width; text: "拍摄结束后仅保存一张合成 RAW 和 JPEG／HEIF"; color: Constants.popupTextDisabledColor; font.pixelSize: 18 * 1.6 * Constants.scaleFactor; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap }
                    }
                    delegate: TextValueRow {
                        width: list.width
                        text: model.text1
                        enabled: model.itemEnabled
                        valueText: ValueFormatter.getDisplayAltValue(model.itemName, model.propValue)
                        pixelSize: 25 * 1.6 * Constants.scaleFactor
                        labelFontWeight: list.labelFontWeight
                        valueFontWeight: list.valueFontWeight
                    }
                }
            }
        }

        MenuSwipe {
            id: swipeArea
            objectName: "PopoverDriveMode_swipe_area"
            property string settingsList: ""
            property bool extensionPage: true
            readonly property bool preventSwipe: menuLoader.item &&
                                                 menuLoader.item.preventSwipe
            anchors.fill: parent
            loader: menuLoader
            parentItem: parent
            enabled: show && menuLoader.active &&
                     !mainMenuViewModel.screenEVF &&
                     !preventSwipe
            filterChildren: true
            deactivateLoaderWhenHidden: !extensionPage
            animateLoader: true

            Keys.onPressed: (event)=> {
                if (MKeys.pressedFnListAcc([KeyFn.Abort, KeyFn.JstkEscape], event)) {
                    show = false
                } else if (MKeys.pressedFnAcc(KeyFn.Right, event)) {
                    // block key
                }
            }

            HblLoader {
                id: menuLoader
                objectName: "PopoverDriveMode_menuLoader"
                active: false
                // 滑出动画仍可绘制，但隐藏的参数页不应挡住下一次简介触摸。
                enabled: swipeArea.show
                asynchronous: !swipeArea.extensionPage
                readonly property bool popupActive: item && item.popupActive

                anchors {
                    top: parent.top
                    bottom: parent.bottom
                }
                width: parent.width
                onWidthChanged: updatePosition()
                onXChanged: {
                    if (swipeArea.isXAtScreenEdge()) {
                        swipeArea.animateLoader = true
                    }
                }

                function updatePosition()
                {
                    // When changing to/from EVF then move x to new position without animation
                    if (!swipeArea.isXAtScreenEdge()) {
                        swipeArea.animateLoader = false
                    }
                }

                source: swipeArea.extensionPage ? "file:///system/etc/x2d-pixel-overclock/X2dMultishotSettings.qml" : "qrc:/app/qml/mainmenu/SettingsGeneric.qml"
                arguments: { "externalMenu": true }

                onLoaded: {
                    if (swipeArea.extensionPage) {
                        // 页面可能先预加载，Host随后才连接控制器；不能缓存当时的null。
                        menuLoader.item.controller = Qt.binding(function(){ return root.multishotController })
                        menuLoader.item.viewModel = Qt.binding(function(){ return root.mainMenuViewModel })
                        menuLoader.item.stackSettings = Qt.binding(function(){ return right_side.isStack })
                        menuLoader.item.stackMethod = Qt.binding(function(){ return root.stackMethod })
                        menuLoader.item.averageNoiseReduction = Qt.binding(function(){ return root.averageNoiseReduction })
                        menuLoader.item.stackTotalSeconds = Qt.binding(function(){ return root.stackTotalSeconds })
                        menuLoader.item.stackInitialDelay = Qt.binding(function(){ return root.stackInitialDelay })
                        menuLoader.item.stackTargetIso = Qt.binding(function(){ return root.stackTargetIso })
                        menuLoader.item.stackFrameCount = Qt.binding(function(){ return root.stackFrameCount })
                        menuLoader.item.stackGapSeconds = Qt.binding(function(){ return root.stackGapSeconds })
                        menuLoader.item.exposureModel = Qt.binding(function(){ return root.exposureModel })
                        return
                    }
                    menuLoader.item.viewModel = Qt.binding( function() { return root.mainMenuViewModel })
                    menuLoader.item.populateModel(0, swipeArea.settingsList)
                    menuLoader.item.menuLabel = Qt.binding( function() { return qsTranslate("DriveModes", list.headerText) })

                    if (!swipeArea.show) {
                        // Close menu after being loaded in onCompleted
                        menuLoader.active = false
                    }
                }

                Connections {
                    target: menuLoader.item
                    ignoreUnknownSignals: true
                    function onClose()
                    {
                        swipeArea.showLoader(false)
                    }
                    function onCloseExternal()
                    {
                        swipeArea.showLoader(false)
                        root.closeDriveMode()
                    }
                }

                // Border for loaded menu
                Rectangle {
                    id: menuBorder

                    anchors.fill: parent
                    color: "transparent"
                    border.width: Constants.popupBorderWidth
                    border.color: Constants.popupBorderColor
                }
            }
        }

        Connections {
            target: menuLoader.item
            ignoreUnknownSignals: true
            function onStackMethodChosen(value) { root.stackMethodChosen(value) }
            function onAverageNoiseReductionChosen(value) { root.averageNoiseReductionChosen(value) }
            function onStackTotalSecondsChosen(value) { root.stackTotalSecondsChosen(value) }
        function onStackInitialDelayChosen(value) { root.stackInitialDelayChosen(value) }
        function onStackTargetIsoChosen(value) { root.stackTargetIsoChosen(value) }
        }
        states: [
            State {
                name: "show_dialog"
                when: swipeArea.show
                PropertyChanges { target: menuLoader; focus: true;}
                StateChangeScript { script: {
                        menuLoader.active = true
                    }
                }
            },
            State {
                name: ""
                PropertyChanges { target: grid; focus: true; restoreEntryValues: false }
            }
        ]
    }

    ListModel {
        id: gridModel
        ListElement {
            icon: "image://svg/ic_controlscreen_drive_mode_single_popup"
            disabledIcon: ""
            text: QT_TRANSLATE_NOOP("DriveModes", "Single")
            description: QT_TRANSLATE_NOOP("DriveModes", "Single Drive Mode")
            settingsList: ""
            drivemode: HblmTypes.E_DriveModes_Single
        }
        ListElement {
            icon: "image://svg/ic_controlscreen_drive_mode_continuous_popup"
            disabledIcon: ""
            text: QT_TRANSLATE_NOOP("DriveModes", "Continuous")
            description: QT_TRANSLATE_NOOP("DriveModes", "Continuous Drive Mode")
            settingsList: ""
            drivemode: HblmTypes.E_DriveModes_Continuous
        }
        ListElement {
            icon: "image://svg/ic_controlscreenpic_drive_mode_selftimer_popup"
            disabledIcon: ""
            text: QT_TRANSLATE_NOOP("DriveModes", "Self-Timer")
            description: ""
            settingsList: "cameraSettingsSelfTimer"
            drivemode: HblmTypes.E_DriveModes_SelfTimer
        }
        ListElement {
            icon: "image://svg/ic_controlscreenpic_drive_mode_interval_popup"
            disabledIcon: "image://svg/ic_controlscreenpic_drive_mode_interval_popup_disable"
            text: QT_TRANSLATE_NOOP("DriveModes", "Interval")
            description: ""
            settingsList: "cameraSettingsInterval"
            drivemode: HblmTypes.E_DriveModes_Interval
        }
        ListElement {
            icon: "image://svg/ic_controlscreen_drive_mode_exposure_bracketing_popup"
            disabledIcon: "image://svg/ic_controlscreen_drive_mode_exposure_bracketing_popup_disable"
            text: QT_TRANSLATE_NOOP("DriveModes", "Exposure Bracketing")
            description: ""
            settingsList: "cameraSettingsExposureBracketing"
            drivemode: HblmTypes.E_DriveModes_ExposureBracketing
        }
        ListElement {
            icon: "image://svg/ic_controlscreen_drive_mode_focus_bracketing_popup"
            disabledIcon: "image://svg/ic_controlscreen_drive_mode_focus_bracketing_popup_disabled"
            text: QT_TRANSLATE_NOOP("DriveModes", "Focus Bracketing")
            description: ""
            settingsList: "cameraSettingsFocusBracketing"
            drivemode: HblmTypes.E_DriveModes_FocusBracketing
        }

        ListElement {
            icon: "file:///system/etc/x2d-pixel-overclock/multishot.svg"
            disabledIcon: ""
            text: "像素超频"
            description: "支持25MP、四亿及十六亿像素。点击右侧参数进行设置。"
            settingsList: ""
            drivemode: -1
            extensionMode: "multishot"
        }
        ListElement {
            icon: "file:///system/etc/x2d-pixel-overclock/stack.svg"
            disabledIcon: "file:///system/etc/x2d-pixel-overclock/stack-disabled.svg"
            text: "RAW 堆栈"
            description: "最大值、最小值、平均值。点击右侧设置总时间；快门开始，完成或时间到后保存。"
            settingsList: ""
            drivemode: -2
            extensionMode: "stack"
        }
    }

    Component.onCompleted: {
        // Loader is slow the first time so start immediately to avoid penalty later.
        menuLoader.active = true
    }
    Component.onDestruction: {
        SubMenu.closeMenu(0)
    }
}
