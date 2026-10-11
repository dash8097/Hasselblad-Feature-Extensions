import QtQuick 2.5

Canvas {
    width:22;height:36
    property bool forward:false
    onForwardChanged:requestPaint()
    onPaint:{
        var c=getContext("2d");c.clearRect(0,0,width,height)
        c.strokeStyle="white";c.lineWidth=3.5;c.lineCap="round";c.lineJoin="round"
        c.beginPath();c.moveTo(forward?3:19,3);c.lineTo(forward?18:4,18);c.lineTo(forward?3:19,33);c.stroke()
    }
}
