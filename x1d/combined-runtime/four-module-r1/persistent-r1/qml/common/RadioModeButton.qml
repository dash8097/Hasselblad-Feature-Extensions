import QtQuick 2.5

Item {
    id: button
    property var adapter: typeof hblNative !== "undefined" ? hblNative : null
    readonly property int mode: adapter ? adapter.radioMode : 0
    readonly property bool switching: adapter ? adapter.radioModeBusy : true
    readonly property bool verified: adapter ? adapter.radioModeVerified : false
    function advance() {
        if(adapter && verified && !switching)
            adapter.command=JSON.stringify({op:"radioMode",mode:(mode+1)%3})
    }
    width: 70; height: 67
    Canvas {
        id: icon
        width: 30; height: 30; anchors.horizontalCenter: parent.horizontalCenter; y: 7
        opacity: button.switching ? 0.4 : 1
        onPaint: {
            var c=getContext("2d");c.clearRect(0,0,width,height)
            c.strokeStyle="white";c.fillStyle="white";c.lineWidth=2.3;c.lineCap="round"
            if(button.mode===2) {
                c.beginPath();c.moveTo(17,2);c.lineTo(5,17);c.lineTo(13,17)
                c.lineTo(10,28);c.lineTo(25,12);c.lineTo(17,12);c.closePath();c.fill()
            } else if(button.mode===1) {
                for(var r=8;r<=24;r+=8){c.beginPath();c.arc(15,28,r,-2.3,-0.84);c.stroke()}
                c.beginPath();c.arc(15,26,2,0,Math.PI*2);c.fill()
            } else {
                c.beginPath();c.arc(15,16,10,-1.05,4.19);c.stroke()
                c.beginPath();c.moveTo(15,3);c.lineTo(15,15);c.stroke()
            }
        }
    }
    onModeChanged: icon.requestPaint()
    Text {
        anchors.horizontalCenter: parent.horizontalCenter;anchors.bottom: parent.bottom;anchors.bottomMargin: 5
        color: "white";font.pixelSize: 15
        text: button.switching ? "切换中" : !button.verified ? "未就绪" : ["关","Wi-Fi","引闪"][button.mode]
    }
    MouseArea {
        anchors.fill: parent
        enabled: button.adapter && button.verified && !button.switching
        onClicked: button.advance()
    }
}
