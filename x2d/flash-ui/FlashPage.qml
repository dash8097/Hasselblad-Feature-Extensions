import QtQuick 2.5

// 纯界面与本地设置草稿。连接状态由适配层提供；本组件不调用相机或无线接口。
Rectangle {
    id: page
    objectName: "FormalFlashPage"
    color: "black"
    property string iconBase: "icons/"
    property string fontName: "sans-serif"
    property bool pageActive: false
    property bool connected: false
    property bool masterEnabled: false
    property bool busy: false
    property bool canTest: false
    // 独立发送偏好默认开启；总开关仍等待适配层确认，不在启动时自动开启。
    property bool sendPowerUpdates: true
    property bool sendFlashSync: true
    readonly property bool powerUpdateAllowed: connected && masterEnabled && !busy && sendPowerUpdates
    readonly property bool flashSyncAllowed: connected && masterEnabled && !busy && sendFlashSync
    property string stateText: connected ? (masterEnabled ? "已开启" : "已关闭") : "未连接"
    property string errorText: ""
    property string apertureText: "F/—"
    property string shutterSpeedText: "—"
    property string isoText: "ISO —"
    property url batterySource: ""
    property int channel: 5
    property int wirelessId: 5
    property string wirelessField: "channel"
    property string wirelessInput: ""
    property bool wirelessInputFresh: true
    property bool wirelessOff: false
    readonly property bool wirelessInputValid: wirelessField==="id" && wirelessOff ||
        (/^[0-9]{1,2}$/.test(wirelessInput) && Number(wirelessInput)>=1 && Number(wirelessInput)<=(wirelessField==="channel" ? 32 : 99))
    property string syncText: "—"
    property string shutterText: "—"
    property bool hasDraftChanges: false
    property int selectedGroup: 3
    property var visibleGroups: [0, 1, 2, 3, 4]
    readonly property int visibleGroupCount: visibleGroups.length
    readonly property real groupScrollY: groupList.contentY
    property string screen: "groups"
    property bool popupOpen: screen !== "groups"
    property bool thirdStopSteps: true
    readonly property string adjustmentStepText: thirdStopSteps ? "0.3 EV" : "0.1 EV"
    property int groupAdjustmentSteps: 0
    readonly property int groupAdjustmentTenths: thirdStopSteps ? Math.round(groupAdjustmentSteps*10/3) : groupAdjustmentSteps
    property bool applyingGroupAdjustment: false
    property bool powerGestureActive: false
    property real swipeSpanEv: 5
    property var lampStates: [false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false]
    readonly property int currentPower: groups.get(selectedGroup).tenthStops
    readonly property bool currentGroupEnabled: groups.get(selectedGroup).active
    readonly property bool hasActiveGroups: {
        for (var i=0;i<groups.count;i++) if (groups.get(i).active) return true
        return false
    }
    property real unit: Math.min(width/640, height/480)
    signal backRequested()
    signal enabledRequested(bool value)
    signal testRequested()
    signal groupDraftChanged(int group, bool active, int tenthStops)
    signal modelingLampDraftChanged(int group, bool value)
    signal deliveryOptionsChanged(bool powerUpdates, bool flashSync)
    signal adjustmentStepRequested(bool thirdStops)
    signal wirelessConfigurationRequested(int channel, int wirelessId)
    signal powerUpdateRequested(int group, bool active, int tenthStops)
    signal flashSyncRequested()
    FlashStyle { id: flashStyle }

    // 0..80 仅为界面草稿范围；适配层须按实际灯型能力收窄，不能据此宣称灯支持 1/256。
    property int minimumPower: 0
    property int maximumPower: 80
    ListModel {
        id: groups
        ListElement { letter: "A"; active: false; tenthStops: 40 }
        ListElement { letter: "B"; active: false; tenthStops: 40 }
        ListElement { letter: "C"; active: false; tenthStops: 40 }
        ListElement { letter: "D"; active: false; tenthStops: 40 }
        ListElement { letter: "E"; active: false; tenthStops: 40 }
        ListElement { letter: "F"; active: false; tenthStops: 40 }
        ListElement { letter: "0"; active: false; tenthStops: 40 }
        ListElement { letter: "1"; active: false; tenthStops: 40 }
        ListElement { letter: "2"; active: false; tenthStops: 40 }
        ListElement { letter: "3"; active: false; tenthStops: 40 }
        ListElement { letter: "4"; active: false; tenthStops: 40 }
        ListElement { letter: "5"; active: false; tenthStops: 40 }
        ListElement { letter: "6"; active: false; tenthStops: 40 }
        ListElement { letter: "7"; active: false; tenthStops: 40 }
        ListElement { letter: "8"; active: false; tenthStops: 40 }
        ListElement { letter: "9"; active: false; tenthStops: 40 }
    }
    function powerText(value) {
        var stop = Math.floor(value / 10)
        return "1/" + Math.pow(2, 8-stop)
    }
    function fractionText(value) { return value%10 ? "+0." + (value%10) : "" }
    function setGroup(index, active, value, asDraft) {
        if (index < 0 || index >= groups.count || Math.floor(index)!==index ||
            !isFinite(value) || Math.floor(value)!==value || value<minimumPower || value>maximumPower) return false
        var previous=groups.get(index)
        if (previous.active===active && previous.tenthStops===value) return true
        if (!applyingGroupAdjustment) groupAdjustmentSteps=0
        groups.setProperty(index, "active", active)
        groups.setProperty(index, "tenthStops", value)
        if (asDraft) {
            hasDraftChanges=true; groupDraftChanged(index, active, value)
            requestPowerUpdate(index)
        }
        return true
    }
    function setDeliveryOptions(powerUpdates, flashSync) {
        if (typeof powerUpdates!=="boolean" || typeof flashSync!=="boolean") return false
        if (sendPowerUpdates===powerUpdates && sendFlashSync===flashSync) return true
        sendPowerUpdates=powerUpdates; sendFlashSync=flashSync
        // 改偏好本身不发功率、不试闪，也不补发关闭期间的旧事件。
        deliveryOptionsChanged(powerUpdates, flashSync)
        return true
    }
    function requestPowerUpdate(index) {
        if (!powerUpdateAllowed || index<0 || index>=groups.count || Math.floor(index)!==index) return false
        var g=groups.get(index)
        powerUpdateRequested(index, g.active, g.tenthStops)
        return true
    }
    function requestFlashSync() {
        // 仅供未来曝光适配层调用。无线同步关闭时不触发外部同步流程。
        if (!flashSyncAllowed) return false
        flashSyncRequested()
        return true
    }
    function requestTest() {
        if (!flashSyncAllowed || !canTest || !hasActiveGroups) return false
        testRequested()
        return true
    }
    function setAdjustmentStep(thirdStops, asDraft) {
        if (typeof thirdStops!=="boolean") return false
        if (thirdStopSteps===thirdStops) return true
        thirdStopSteps=thirdStops; groupAdjustmentSteps=0
        if (asDraft) adjustmentStepRequested(thirdStops)
        return true
    }
    function steppedPower(value, direction) {
        if (direction!==-1 && direction!==1) return NaN
        if (!thirdStopSteps) return value+direction
        // 三分之一档显示 0、0.3、0.7、1；不把每步固定加 0.3 累积成 0.9。
        // 从十分之一档切换后，只在下一次按加减时移到对应方向的相邻档。
        if (direction>0) {
            for (var up=0;up<=24;up++) {
                var higher=Math.round(up*10/3)
                if (higher>value) return higher
            }
            return maximumPower+1
        }
        for (var down=24;down>=0;down--) {
            var lower=Math.round(down*10/3)
            if (lower<value) return lower
        }
        return minimumPower-1
    }
    function adjustPower(direction) {
        var g=groups.get(selectedGroup)
        return setGroup(selectedGroup, g.active, Math.max(minimumPower, Math.min(maximumPower, steppedPower(g.tenthStops,direction))), true)
    }
    function quickAdjust(index, direction) {
        if (index<0 || index>=groups.count) return false
        var g=groups.get(index)
        if (!g.active) return false
        return setGroup(index, true, Math.max(minimumPower, Math.min(maximumPower, steppedPower(g.tenthStops,direction))), true)
    }
    function swipePower(index, startValue, steps) {
        if (index<0 || index>=groups.count || !isFinite(steps) || Math.floor(steps)!==steps) return false
        var g=groups.get(index)
        if (!g.active) return false
        var next=startValue, direction=steps<0 ? -1 : 1
        // 始终由按下时的数值计算；回滑不因舍入或重复事件累计漂移。
        for (var n=0;n<Math.min(Math.abs(steps),80);n++)
            next=Math.max(minimumPower,Math.min(maximumPower,steppedPower(next,direction)))
        return setGroup(index,true,next,true)
    }
    function groupPower(index) { return groups.get(index).tenthStops }
    function groupActive(index) { return groups.get(index).active }
    function toggleGroup(index) {
        var g=groups.get(index)
        return setGroup(index,!g.active,g.tenthStops,true)
    }
    function toggleLamp(index) {
        if (index<0 || index>=groups.count || Math.floor(index)!==index) return false
        var next=lampStates.slice(0); next[index]=!next[index]; lampStates=next
        hasDraftChanges=true; modelingLampDraftChanged(index,next[index]); return true
    }
    function canAdjustVisiblePower(direction) {
        if (!visibleGroups.length || (direction!==-1 && direction!==1)) return false
        for(var n=0;n<visibleGroups.length;n++) {
            var power=steppedPower(groups.get(visibleGroups[n]).tenthStops,direction)
            if(power<minimumPower || power>maximumPower) return false
        }
        return true
    }
    function adjustVisiblePower(direction) {
        // 达到边界时整批不动，保留所选组之间的功率差。隐藏组及组开关不变。
        if(!canAdjustVisiblePower(direction)) return false
        applyingGroupAdjustment=true
        try {
            for(var n=0;n<visibleGroups.length;n++) {
                var index=visibleGroups[n], g=groups.get(index)
                setGroup(index,g.active,steppedPower(g.tenthStops,direction),true)
            }
        } finally {
            applyingGroupAdjustment=false
        }
        groupAdjustmentSteps+=direction
        return true
    }
    function groupAdjustmentText() {
        return (groupAdjustmentTenths>0 ? "+" : "")+(groupAdjustmentTenths/10).toFixed(1)
    }
    function groupIsVisible(index) { return visibleGroups.indexOf(index)>=0 }
    function toggleVisibleGroup(index) {
        if (index<0 || index>=groups.count || Math.floor(index)!==index) return false
        var next=visibleGroups.slice(0), position=next.indexOf(index)
        if (position>=0) next.splice(position,1); else next.push(index)
        next.sort(function(a,b) { return a-b })
        visibleGroups=next
        groupAdjustmentSteps=0
        groupList.contentY=0
        return true
    }
    function openGroup(index) { selectedGroup=index; screen="power" }
    function openWireless(field) {
        if (field!=="channel" && field!=="id") return false
        wirelessField=field
        wirelessInput=String(field==="channel" ? channel : wirelessId)
        wirelessOff=field==="id" && wirelessId===0
        wirelessInputFresh=true; screen="wireless"; return true
    }
    function wirelessKey(key) {
        if (screen!=="wireless") return false
        if (key==="清除") { wirelessInput=""; wirelessOff=false; wirelessInputFresh=false; return true }
        if (key==="退格") { wirelessInput=wirelessOff ? "" : wirelessInput.slice(0,-1); wirelessOff=false; wirelessInputFresh=false; return true }
        if (!/^[0-9]$/.test(key)) return false
        if (wirelessInputFresh || wirelessOff) wirelessInput=""
        wirelessInputFresh=false; wirelessOff=false
        if (wirelessInput.length>=2) return false
        wirelessInput+=key; return true
    }
    function confirmWireless() {
        if (screen!=="wireless" || !wirelessInputValid) return false
        var value=wirelessOff ? 0 : Number(wirelessInput)
        wirelessConfigurationRequested(wirelessField==="channel" ? value : channel, wirelessField==="id" ? value : wirelessId)
        screen="groups"; return true
    }
    function closeDetail() { screen="groups" }
    function requestBack() { if (popupOpen) closeDetail(); else backRequested() }
    onPageActiveChanged: if (!pageActive) screen="groups"
    // 页面显示与隐藏不会准备无线、启用引闪、提交草稿或触发试闪。

    Item {
        id: canvas; width: 640; height: 480
        scale: page.unit; transformOrigin: Item.TopLeft
        x: (page.width-width*page.unit)/2; y: (page.height-height*page.unit)/2
        MouseArea { anchors.fill: parent }

        FlashIconButton {
            objectName: "FlashBack"; x: 4; y: 8; width: 60; height: 56
            source: "icons/back.svg"; iconSize: 24
            onClicked: page.requestBack()
        }
        Image { x: 70; y: 24; width: 22; height: 25; visible: page.screen!=="groups"; fillMode: Image.PreserveAspectFit; source: "icons/flash.svg" }
        FlashText {
            visible: page.screen!=="groups"
            x: 104; y: 10; width: 140; height: 52; font.pixelSize: flashStyle.titleSize; font.family: page.fontName
            text: page.screen==="groups" ? "无线引闪" : page.screen==="power" ? groups.get(page.selectedGroup).letter+"  组" : page.screen==="selection" ? "显示组别" : page.screen==="wireless" ? (page.wirelessField==="channel" ? "频道" : "无线 ID") : "引闪设置"
        }
        Row {
            x: 326; y: 10; spacing: 8; height: 52; visible: page.screen!=="wireless"
            Item {
                width: 72; height: parent.height
                FlashValueText { width: parent.width; height: parent.height; text: "CH "+page.channel; font.pixelSize: flashStyle.channelSize-2; font.family: page.fontName }
                MouseArea { objectName: "FlashChannel"; anchors.fill: parent; onClicked: page.openWireless("channel") }
            }
            Item {
                width: 84; height: parent.height
                FlashValueText { width: parent.width; height: parent.height; text: "ID "+(page.wirelessId===0 ? "OFF" : page.wirelessId); font.pixelSize: flashStyle.channelSize-2; font.family: page.fontName }
                MouseArea { objectName: "FlashWirelessId"; anchors.fill: parent; onClicked: page.openWireless("id") }
            }
        }
        Item {
            objectName: "FlashExposureSummary"; x: 70; y: 10; width: 246; height: 52; visible: page.screen==="groups"
            TextMetrics {
                id: exposureMetrics
                font.family: page.fontName; font.pixelSize: 21; font.weight: Font.Light
                text: page.apertureText+"  "+page.shutterSpeedText+"  "+page.isoText
            }
            FlashValueText {
                objectName: "FlashExposureLine"; width: parent.width; height: parent.height
                text: exposureMetrics.text
                font.pixelSize: Math.max(17,Math.min(21,Math.floor(21*width/Math.max(1,exposureMetrics.advanceWidth))))
                font.family: page.fontName
                wrapMode: Text.NoWrap
            }
        }
        FlashIconButton {
            objectName: "FlashMaster"; x: 500; y: 3; width: 70; height: 67
            visible: page.screen!=="wireless"
            source: "icons/power.svg"; label: page.masterEnabled ? "已开启" : "已关闭"
            iconOpacity: page.masterEnabled ? 1 : flashStyle.disabledOpacity
            available: page.masterEnabled || (page.connected && !page.busy); fontName: page.fontName
            onClicked: page.enabledRequested(!page.masterEnabled)
        }
        Item {
            objectName: "FlashBatterySlot"; x: 574; y: 20; width: 50; height: 32
            Loader {
                objectName: "FlashBattery"
                anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
                height: 30
                source: page.batterySource
                onLoaded: {
                    item.batteryWidth=19
                    item.lineWidth=2.5
                    item.sizeFactor=19/23
                }
            }
        }
        Rectangle { x: flashStyle.pageInset; y: 76; width: flashStyle.contentWidth; height: 1; color: "white"; opacity: flashStyle.frameOpacity; visible: page.screen!=="settings" }

        ListView {
            id: groupList; objectName: "FlashGroupList"
            x: flashStyle.pageInset; y: flashStyle.contentTop; width: flashStyle.contentWidth; height: flashStyle.contentHeight
            visible: page.screen==="groups"; clip: true
            model: page.visibleGroups
            boundsBehavior: Flickable.StopAtBounds
            flickableDirection: Flickable.VerticalFlick
            snapMode: ListView.SnapToItem
            function settleRows() {
                var target=Math.max(0,Math.min(Math.max(0,contentHeight-height),Math.round(contentY/58)*58))
                if (Math.abs(contentY-target)>0.5) { rowSnap.to=target; rowSnap.restart() }
            }
            onMovementEnded: settleRows()
            NumberAnimation { id: rowSnap; target: groupList; property: "contentY"; duration: 140; easing.type: Easing.OutCubic }
            delegate: Item {
                id: groupRow
                property int groupIndex: modelData
                property var groupValue: groups.get(groupIndex)
                objectName: "FlashGroup"+groupValue.letter
                width: 608; height: 58
                Item {
                    id: groupContent; width: parent.width; height: parent.height-1
                    opacity: groupRow.groupValue.active ? 1 : 0.48
                    FlashValueText { x: 20; width: 52; height: groupContent.height; text: groupRow.groupValue.letter; font.pixelSize: flashStyle.groupSize; font.family: page.fontName }
                    FlashValueText { x: 70; width: 37; height: groupContent.height; text: groupRow.groupValue.active ? "M" : "—"; font.pixelSize: flashStyle.secondarySize+2; font.family: page.fontName }
                    MouseArea { objectName: "FlashGroupToggle"+groupRow.groupValue.letter; x: 0; width: 106; height: groupContent.height; onClicked: page.toggleGroup(groupRow.groupIndex) }
                    Item {
                        x: 109; width: 64; height: groupContent.height
                        FlashValueText { anchors.horizontalCenter: parent.horizontalCenter; height: parent.height; text: "−"; font.pixelSize: flashStyle.groupSize+4; font.family: page.fontName }
                        MouseArea { objectName: "FlashQuickMinus"+groupRow.groupValue.letter; anchors.fill: parent; enabled: groupRow.groupValue.active; onClicked: page.quickAdjust(groupRow.groupIndex,-1) }
                    }
                    Item {
                        x: 173; width: 240; height: groupContent.height
                        Row {
                            anchors.centerIn: parent; spacing: 7; height: parent.height
                            FlashValueText { width: implicitWidth; height: parent.height; text: groupRow.groupValue.active ? page.powerText(groupRow.groupValue.tenthStops) : "OFF"; font.pixelSize: flashStyle.powerSize; font.family: page.fontName }
                            FlashValueText { width: implicitWidth; height: parent.height; text: groupRow.groupValue.active ? page.fractionText(groupRow.groupValue.tenthStops) : ""; visible: text.length>0; font.pixelSize: flashStyle.bodySize; font.family: page.fontName }
                        }
                    }
                    MouseArea {
                        objectName: "FlashGroupDetail"+groupRow.groupValue.letter; x: 173; width: 240; height: groupContent.height
                        preventStealing: true
                        property real pressX: 0
                        property real pressY: 0
                        property real startScroll: 0
                        property real lastY: 0
                        property real lastMoveAt: 0
                        property real velocityY: 0
                        property int startValue: 0
                        property bool moved: false
                        property bool verticalGesture: false
                        onPressed: {
                            var position=mapToItem(canvas,mouse.x,mouse.y)
                            pressX=position.x; pressY=position.y; lastY=position.y; lastMoveAt=Date.now(); velocityY=0
                            groupList.cancelFlick(); rowSnap.stop(); startScroll=groupList.contentY
                            startValue=groupRow.groupValue.tenthStops; moved=false; verticalGesture=false
                            page.powerGestureActive=true
                        }
                        onPositionChanged: if (pressed) {
                            var position=mapToItem(canvas,mouse.x,mouse.y)
                            var distance=position.x-pressX
                            var vertical=position.y-pressY
                            if (!moved && Math.abs(vertical)>=8 && Math.abs(vertical)>Math.abs(distance)) {
                                verticalGesture=true
                            }
                            if (verticalGesture) {
                                groupList.contentY=Math.max(0,Math.min(Math.max(0,groupList.contentHeight-groupList.height),startScroll-vertical))
                                var now=Date.now(), elapsed=now-lastMoveAt
                                if (elapsed>0) velocityY=0.4*velocityY+0.6*(position.y-lastY)*1000/elapsed
                                lastY=position.y; lastMoveAt=now
                            }
                            if (!verticalGesture && Math.abs(distance)>=8) moved=true
                            if (!verticalGesture && moved && groupRow.groupValue.active) {
                                var raw=distance/width*page.swipeSpanEv*(page.thirdStopSteps ? 3 : 10)
                                var steps=raw<0 ? Math.ceil(raw) : Math.floor(raw)
                                page.swipePower(groupRow.groupIndex,startValue,steps)
                            }
                        }
                        onReleased: {
                            page.powerGestureActive=false
                            if (verticalGesture && Date.now()-lastMoveAt<100 && Math.abs(velocityY)>50)
                                groupList.flick(0,velocityY)
                            else if (verticalGesture) groupList.settleRows()
                        }
                        onCanceled: { moved=true; page.powerGestureActive=false }
                        onClicked: if (!moved && !verticalGesture) page.openGroup(groupRow.groupIndex)
                        Component.onDestruction: page.powerGestureActive=false
                    }
                    Item {
                        x: 413; width: 64; height: groupContent.height
                        FlashValueText { anchors.horizontalCenter: parent.horizontalCenter; height: parent.height; text: "+"; font.pixelSize: flashStyle.groupSize+4; font.family: page.fontName }
                        MouseArea { objectName: "FlashQuickPlus"+groupRow.groupValue.letter; anchors.fill: parent; enabled: groupRow.groupValue.active; onClicked: page.quickAdjust(groupRow.groupIndex,1) }
                    }
                    Item {
                        x: 481; width: 58; height: groupContent.height
                        FlashLampIcon { anchors.centerIn: parent; width: 28; height: 28; lit: page.lampStates[groupRow.groupIndex] }
                        MouseArea { objectName: "FlashLamp"+groupRow.groupValue.letter; anchors.fill: parent; onClicked: page.toggleLamp(groupRow.groupIndex) }
                    }
                    Item {
                        x: 562; width: 42; height: groupContent.height
                        Image { anchors.centerIn: parent; width: 22; height: 20; source: "icons/chevron.svg"; fillMode: Image.PreserveAspectFit }
                        MouseArea { objectName: "FlashDetailArrow"+groupRow.groupValue.letter; anchors.fill: parent; onClicked: page.openGroup(groupRow.groupIndex) }
                    }
                }
                Rectangle { x: 0; y: groupRow.height-1; width: flashStyle.contentWidth; height: 1; color: "white"; opacity: flashStyle.dividerOpacity }
            }
        }
        FlashText {
            x: 32; y: 210; width: 576; height: 60; text: "选择要显示的组别"; horizontalAlignment: Text.AlignHCenter
            visible: page.screen==="groups" && page.visibleGroupCount===0; opacity: 0.6; font.family: page.fontName
        }
        Rectangle {
            x: 630; y: groupList.y+groupList.height*(groupList.contentY/Math.max(groupList.height,groupList.contentHeight))
            width: 2; height: Math.min(groupList.height,groupList.height*groupList.height/Math.max(groupList.height,groupList.contentHeight)); color: "white"; opacity: 0.35
            visible: page.screen==="groups" && page.visibleGroupCount>5
        }

        Item {
            objectName: "FlashGroupSelection"; x: flashStyle.pageInset; y: flashStyle.contentTop; width: flashStyle.contentWidth; height: flashStyle.contentHeight
            visible: page.screen==="selection"
            FlashText { x: 20; y: 0; width: 568; height: 36; text: "已选  "+page.visibleGroupCount+"  组"; font.pixelSize: flashStyle.secondarySize; opacity: flashStyle.mutedOpacity; font.family: page.fontName }
            Grid {
                x: 12; y: 41; columns: 4; rowSpacing: 7; columnSpacing: 10
                Repeater {
                    model: groups
                    delegate: Item {
                        width: 136; height: 54
                        property bool selected: page.groupIsVisible(index)
                        opacity: selected ? 1 : 0.45
                        FlashText { anchors.centerIn: parent; text: letter; font.pixelSize: 27; font.family: page.fontName }
                        Image { x: 6; y: 8; width: 7; height: 38; fillMode: Image.Stretch; source: "icons/bracket.svg"; visible: parent.selected }
                        Image { x: 123; y: 8; width: 7; height: 38; fillMode: Image.Stretch; source: "icons/bracket.svg"; visible: parent.selected }
                        MouseArea { objectName: "FlashSelectGroup"+letter; anchors.fill: parent; onClicked: page.toggleVisibleGroup(index) }
                    }
                }
            }
        }

        Item {
            objectName: "FlashPowerEditor"; x: flashStyle.pageInset; y: flashStyle.contentTop; width: flashStyle.contentWidth; height: flashStyle.contentHeight
            visible: page.screen==="power"
            Item {
                x: 20; y: 6; width: 110; height: 62
                FlashText { anchors.centerIn: parent; text: groups.get(page.selectedGroup).active ? "M" : "OFF"; font.pixelSize: 26; font.family: page.fontName }
                Image { x: 0; y: 10; height: 42; width: 9; fillMode: Image.Stretch; source: "icons/bracket.svg" }
                Image { x: 101; y: 10; height: 42; width: 9; fillMode: Image.Stretch; source: "icons/bracket.svg" }
                MouseArea { objectName: "FlashGroupToggle"; anchors.fill: parent; onClicked: { var g=groups.get(page.selectedGroup); page.setGroup(page.selectedGroup,!g.active,g.tenthStops,true) } }
            }
            FlashText { x: 393; y: 14; width: 195; height: 40; text: "手动功率"; horizontalAlignment: Text.AlignRight; font.pixelSize: flashStyle.secondarySize; opacity: flashStyle.mutedOpacity; font.family: page.fontName }
            Item {
                x: 20; y: 76; width: 568; height: 112
                Row {
                    anchors.centerIn: parent; spacing: 10; height: parent.height
                    FlashValueText { width: implicitWidth; height: parent.height; text: page.powerText(groups.get(page.selectedGroup).tenthStops); font.pixelSize: 80; font.family: page.fontName }
                    FlashValueText { width: implicitWidth; height: parent.height; text: page.fractionText(groups.get(page.selectedGroup).tenthStops); visible: text.length>0; font.pixelSize: 32; font.family: page.fontName }
                }
            }
            Row {
                x: 20; y: 211; spacing: 8
                Repeater {
                    model: ["−", page.adjustmentStepText, "+"]
                    delegate: Item {
                        width: 184; height: 64
                        FlashText { anchors.centerIn: parent; text: modelData; font.pixelSize: index===1 ? flashStyle.titleSize : flashStyle.adjustmentSize; font.family: page.fontName }
                        Rectangle { anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter; width: index===1 ? 86 : 34; height: 1; color: "white"; opacity: 0.5 }
                        MouseArea {
                            objectName: index===0 ? "FlashPowerMinus" : index===1 ? "FlashPowerStep" : "FlashPowerPlus"
                            anchors.fill: parent
                            enabled: index!==1
                            onClicked: page.adjustPower(index===0 ? -1 : 1)
                        }
                    }
                }
            }
        }

        Item {
            objectName: "FlashWirelessEditor"
            x: flashStyle.pageInset; y: flashStyle.contentTop; width: flashStyle.contentWidth; height: flashStyle.contentHeight
            visible: page.screen==="wireless"
            FlashText { x: 20; y: 6; width: 195; height: 45; text: page.wirelessField==="channel" ? "CH  1–32" : "ID  1–99 / OFF"; font.pixelSize: 24; opacity: 0.6; font.family: page.fontName }
            FlashText { objectName: "FlashWirelessInput"; x: 20; y: 54; width: 195; height: 72; text: page.wirelessOff ? "OFF" : page.wirelessInput.length ? page.wirelessInput : "—"; horizontalAlignment: Text.AlignHCenter; font.pixelSize: 56; font.family: page.fontName }
            Rectangle {
                x: 20; y: 140; width: 195; height: 58; radius: 4; visible: page.wirelessField==="id"
                color: page.wirelessOff ? flashStyle.activeColor : "transparent"; border.color: "white"
                FlashText { anchors.fill: parent; text: "关闭"; horizontalAlignment: Text.AlignHCenter; font.pixelSize: 27; font.family: page.fontName }
                MouseArea { objectName: "FlashWirelessOff"; anchors.fill: parent; onClicked: { page.wirelessOff=true; page.wirelessInput=""; page.wirelessInputFresh=true } }
            }
            Rectangle {
                x: 20; y: 218; width: 195; height: 62; radius: 4; color: flashStyle.activeColor; opacity: page.wirelessInputValid ? 1 : 0.3
                FlashText { anchors.fill: parent; text: "确认"; horizontalAlignment: Text.AlignHCenter; font.pixelSize: 27; font.family: page.fontName }
                MouseArea { objectName: "FlashWirelessConfirm"; anchors.fill: parent; enabled: page.wirelessInputValid; onClicked: page.confirmWireless() }
            }
            Grid {
                x: 246; y: 10; columns: 3; rowSpacing: 8; columnSpacing: 10
                Repeater {
                    model: ["1","2","3","4","5","6","7","8","9","清除","0","退格"]
                    delegate: Rectangle {
                        width: 106; height: 60; radius: 4; color: keyTouch.pressed ? "#404040" : "#202020"
                        FlashText { anchors.fill: parent; text: modelData; horizontalAlignment: Text.AlignHCenter; font.pixelSize: index===9 || index===11 ? 24 : 32; font.family: page.fontName }
                        MouseArea { id: keyTouch; objectName: "FlashWirelessKey"+modelData; anchors.fill: parent; onClicked: page.wirelessKey(modelData) }
                    }
                }
            }
        }
        Item {
            objectName: "FlashSettingsPage"
            x: flashStyle.pageInset; y: flashStyle.contentTop; width: flashStyle.contentWidth; height: flashStyle.contentHeight
            visible: page.screen==="settings"
            FlashToggleRow {
                objectName: "FlashSendPowerUpdates"; y: 0; height: 58
                label: "发送功率更新"; checked: page.sendPowerUpdates; fontName: page.fontName
                onToggled: page.setDeliveryOptions(!page.sendPowerUpdates, page.sendFlashSync)
            }
            FlashToggleRow {
                objectName: "FlashSendSync"; y: 58; height: 58
                label: "发送引闪同步"; checked: page.sendFlashSync; fontName: page.fontName
                onToggled: page.setDeliveryOptions(page.sendPowerUpdates, !page.sendFlashSync)
            }
            Item {
                objectName: "FlashAdjustmentStep"; y: 116; width: flashStyle.contentWidth; height: 58
                FlashText { x: 20; width: 256; height: parent.height; text: "调节步长"; font.pixelSize: flashStyle.titleSize; font.family: page.fontName }
                Repeater {
                    model: ["0.3 EV", "0.1 EV"]
                    delegate: Rectangle {
                        x: 324+index*136; y: 6; width: 128; height: 46; radius: 4
                        color: page.thirdStopSteps===(index===0) ? flashStyle.activeColor : "transparent"
                        border.color: "white"; border.width: 1
                        FlashText { anchors.fill: parent; text: modelData; horizontalAlignment: Text.AlignHCenter; font.pixelSize: flashStyle.titleSize; font.family: page.fontName }
                        MouseArea { objectName: index===0 ? "FlashStepThird" : "FlashStepTenth"; anchors.fill: parent; onClicked: page.setAdjustmentStep(index===0,true) }
                    }
                }
            }
            Repeater {
                model: ["同步", "快门"]
                delegate: Item {
                    y: 174+index*58; width: flashStyle.contentWidth; height: 58
                    FlashText { x: 20; width: 256; height: parent.height; text: modelData; font.pixelSize: flashStyle.titleSize; font.family: page.fontName }
                    FlashText { x: 288; width: 300; height: parent.height; text: [page.syncText, page.shutterText][index]; horizontalAlignment: Text.AlignRight; font.pixelSize: flashStyle.titleSize; opacity: flashStyle.mutedOpacity; font.family: page.fontName }
                }
            }
        }
        FlashText {
            objectName: "FlashError"; x: flashStyle.pageInset; y: 376; width: flashStyle.contentWidth; height: 16
            text: page.errorText; visible: text.length>0; color: flashStyle.activeColor
            font.pixelSize: 14; font.family: page.fontName; horizontalAlignment: Text.AlignHCenter
        }
        Rectangle { x: flashStyle.pageInset; y: 392; width: flashStyle.contentWidth; height: 1; color: "white"; opacity: flashStyle.frameOpacity; visible: page.screen!=="settings" }
        FlashIconButton {
            objectName: "FlashChooseGroups"; x: 425; y: flashStyle.footerTop; width: 88; height: flashStyle.footerHeight
            visible: page.screen!=="wireless"
            groupSelector: true; fontName: page.fontName
            label: page.screen==="selection" ? "完成" : "选择分组"
            onClicked: page.screen=page.screen==="selection" ? "groups" : "selection"
        }
        Item {
            x: flashStyle.pageInset; y: flashStyle.footerTop; width: 300; height: flashStyle.footerHeight
            visible: page.screen==="groups"
            FlashText { x: 0; width: 78; height: 64; text: "−"; font.pixelSize: flashStyle.adjustmentSize; horizontalAlignment: Text.AlignHCenter; font.family: page.fontName; opacity: page.canAdjustVisiblePower(-1) ? 1 : 0.3 }
            MouseArea { objectName: "FlashAllMinus"; x: 0; width: 78; height: 70; onClicked: page.adjustVisiblePower(-1) }
            FlashText { objectName: "FlashGroupOffset"; x: 80; width: 124; height: 64; text: page.groupAdjustmentText(); font.pixelSize: 31; horizontalAlignment: Text.AlignHCenter; font.family: page.fontName }
            FlashText { x: 214; width: 78; height: 64; text: "+"; font.pixelSize: flashStyle.adjustmentSize; horizontalAlignment: Text.AlignHCenter; font.family: page.fontName; opacity: page.canAdjustVisiblePower(1) ? 1 : 0.3 }
            MouseArea { objectName: "FlashAllPlus"; x: 214; width: 78; height: 70; onClicked: page.adjustVisiblePower(1) }
        }
        FlashIconButton {
            objectName: "FlashTest"; x: 334; y: flashStyle.footerTop; width: 72; height: flashStyle.footerHeight
            visible: page.screen!=="wireless"
            source: "icons/flash.svg"; label: "试闪"; fontName: page.fontName
            available: page.flashSyncAllowed && page.canTest && page.hasActiveGroups
            onClicked: page.requestTest()
        }
        FlashIconButton {
            objectName: "FlashSettings"; x: 536; y: flashStyle.footerTop; width: 88; height: flashStyle.footerHeight
            visible: page.screen!=="wireless"
            source: "icons/settings.svg"; label: "设置"; fontName: page.fontName
            iconSize: 46
            outlined: true
            onClicked: page.screen=page.screen==="settings" ? "groups" : "settings"
        }
    }
}
