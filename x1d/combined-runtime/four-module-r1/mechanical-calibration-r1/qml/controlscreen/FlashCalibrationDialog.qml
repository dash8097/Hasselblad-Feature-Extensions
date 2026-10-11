import QtQuick 2.5

Item {
    id: dialog
    objectName: "FlashCalibrationDialog"
    property var delays: [5000,5000,6300,6900,6900]
    property bool editable: true
    property string fontName: "Helvetica Neue LT Std"
    property int selected: -1
    property string input: ""
    property bool fresh: true
    readonly property var denominators: [125,250,500,1000,2000]
    readonly property bool inputValid: /^[0-9]{1,4}(\.[0-9]{0,2})?$/.test(input) && Number(input)<=5000
    signal delayRequested(int index, int microseconds)
    signal closeRequested()
    onVisibleChanged: if (visible) selected=-1
    function adjust(index, delta) {
        if (!editable || index<0 || index>=5) return false
        var value=Number(delays[index])+delta
        if (value<0 || value>5000000 || value%10) return false
        delayRequested(index,value); return true
    }
    function edit(index) {
        if (!editable || index<0 || index>=5) return false
        selected=index; input=(Number(delays[index])/1000).toFixed(2); fresh=true; return true
    }
    function key(value) {
        if (selected<0 || !editable) return false
        if (value==="退格") { input=input.slice(0,-1);fresh=false;return true }
        if (fresh) { input="";fresh=false }
        if (value==="." && input.indexOf(".")>=0) return false
        if (value==="." && input==="") input="0"
        var next=input+value
        if (!/^[0-9]{0,4}(\.[0-9]{0,2})?$/.test(next)) return false
        input=next;return true
    }
    function confirm() {
        if (!editable || selected<0 || !inputValid) return false
        delayRequested(selected,Math.round(Number(input)*100)*10)
        selected=-1;return true
    }
    Rectangle { anchors.fill: parent; color: "#B0000000" }
    MouseArea { anchors.fill: parent }
    Rectangle {
        x: 44; y: 40; width: 552; height: 400
        color: "black"; border.color: "#de4200"; border.width: 2
        FlashText {
            x: 20; y: 10; width: 512; height: 38
            text: dialog.selected<0 ? "机械快门延迟标定" : "1/"+dialog.denominators[dialog.selected]+" 秒 · 绝对延迟"
            font.pixelSize: 24; font.family: dialog.fontName; horizontalAlignment: Text.AlignHCenter
        }
        Item {
            x: 20; y: 58; width: 512; height: 320; visible: dialog.selected<0
            Repeater {
                model: 5
                delegate: Item {
                    y: index*46; width: 512; height: 42
                    FlashText { width: 115; height: parent.height; text: "1/"+dialog.denominators[index]+" 秒"; font.pixelSize: 22; font.family: dialog.fontName }
                    Rectangle {
                        x: 126; width: 54; height: 40; radius: 3; color: "#202020"
                        opacity: dialog.editable && dialog.delays[index]>=100 ? 1 : 0.3
                        FlashText { anchors.fill: parent; text: "−"; horizontalAlignment: Text.AlignHCenter; font.pixelSize: 30; font.family: dialog.fontName }
                        MouseArea { objectName: "FlashDelayMinus"+index; anchors.fill: parent; onClicked: dialog.adjust(index,-100) }
                    }
                    Rectangle {
                        x: 190; width: 194; height: 40; color: "transparent"; border.color: "#646464"
                        FlashText { anchors.fill: parent; text: (Number(dialog.delays[index])/1000).toFixed(2)+" ms"; horizontalAlignment: Text.AlignHCenter; font.pixelSize: 24; font.family: dialog.fontName }
                        MouseArea { objectName: "FlashDelayValue"+index; anchors.fill: parent; onClicked: dialog.edit(index) }
                    }
                    Rectangle {
                        x: 394; width: 54; height: 40; radius: 3; color: "#202020"
                        opacity: dialog.editable && dialog.delays[index]<=4999900 ? 1 : 0.3
                        FlashText { anchors.fill: parent; text: "+"; horizontalAlignment: Text.AlignHCenter; font.pixelSize: 30; font.family: dialog.fontName }
                        MouseArea { objectName: "FlashDelayPlus"+index; anchors.fill: parent; onClicked: dialog.adjust(index,100) }
                    }
                }
            }
            FlashText {
                y: 236; width: 512; height: 34
                text: dialog.editable ? "中间档自动插值 · 更慢快门沿用 1/125" : "拍摄处理中，暂不可修改"
                font.pixelSize: 17; font.family: dialog.fontName; opacity: 0.65; horizontalAlignment: Text.AlignHCenter
            }
            Rectangle {
                x: 170; y: 280; width: 172; height: 42; color: "#202020"; border.color: "#646464"
                FlashText { anchors.fill: parent; text: "完成"; font.pixelSize: 22; font.family: dialog.fontName; horizontalAlignment: Text.AlignHCenter }
                MouseArea { objectName: "FlashCalibrationClose"; anchors.fill: parent; onClicked: dialog.closeRequested() }
            }
        }
        Item {
            x: 20; y: 64; width: 512; height: 310; visible: dialog.selected>=0
            FlashText { x: 0; y: 4; width: 180; height: 36; text: "毫秒 · 0–5000"; font.pixelSize: 18; font.family: dialog.fontName; opacity: 0.65; horizontalAlignment: Text.AlignHCenter }
            FlashText { objectName: "FlashDelayInput"; x: 0; y: 50; width: 180; height: 60; text: dialog.input.length ? dialog.input : "—"; font.pixelSize: 32; font.family: dialog.fontName; horizontalAlignment: Text.AlignHCenter }
            Rectangle {
                x: 0; y: 146; width: 180; height: 50; color: "#de4200"; opacity: dialog.inputValid && dialog.editable ? 1 : 0.3
                FlashText { anchors.fill: parent; text: "确认"; font.pixelSize: 22; font.family: dialog.fontName; horizontalAlignment: Text.AlignHCenter }
                MouseArea { objectName: "FlashDelayConfirm"; anchors.fill: parent; onClicked: dialog.confirm() }
            }
            Rectangle {
                x: 0; y: 216; width: 180; height: 50; color: "#202020"
                FlashText { anchors.fill: parent; text: "返回"; font.pixelSize: 22; font.family: dialog.fontName; horizontalAlignment: Text.AlignHCenter }
                MouseArea { objectName: "FlashDelayCancel"; anchors.fill: parent; onClicked: dialog.selected=-1 }
            }
            Grid {
                x: 202; columns: 3; rowSpacing: 7; columnSpacing: 7
                Repeater {
                    model: ["1","2","3","4","5","6","7","8","9",".","0","退格"]
                    delegate: Rectangle {
                        width: 98; height: 64; color: keyTouch.pressed ? "#404040" : "#202020"; radius: 3
                        FlashText { anchors.fill: parent; text: modelData; font.pixelSize: modelData==="退格" ? 22 : 30; font.family: dialog.fontName; horizontalAlignment: Text.AlignHCenter }
                        MouseArea { id: keyTouch; objectName: "FlashDelayKey"+modelData; anchors.fill: parent; onClicked: dialog.key(modelData) }
                    }
                }
            }
        }
    }
}
