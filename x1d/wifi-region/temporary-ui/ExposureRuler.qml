import QtQuick 2.5

Item {
    id:root;width:200;height:52
    property real value:0
    property bool valid:false
    readonly property real indicatorX:10+(Math.max(-2,Math.min(2,value))+2)*(width-20)/4
    Text {
        objectName:"ExposureRulerValue"
        x:Math.max(0,Math.min(root.width-width,root.indicatorX-width/2));y:0
        text:root.valid?((root.value<-2?"◀ ":root.value>2?"▶ ":"")+(root.value>0?"+":"")+root.value.toFixed(1)):"—"
        color:root.valid?"white":"#777";font.pixelSize:22
    }
    Repeater {
        model:13
        Rectangle {
            x:10+index*(root.width-20)/12-width/2;anchors.bottom:parent.bottom
            width:index===6?3:2;height:index===6?22:index%3===0?17:11
            color:root.valid?"white":"#777"
        }
    }
    Rectangle {x:root.indicatorX-2;y:27;width:4;height:7;color:"white";visible:root.valid}
}
