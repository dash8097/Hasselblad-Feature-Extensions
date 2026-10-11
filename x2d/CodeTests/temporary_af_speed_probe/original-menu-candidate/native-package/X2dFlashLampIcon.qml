import QtQuick 2.5

Canvas {
    id: lamp
    property bool lit: false
    onLitChanged: requestPaint()
    onPaint: {
        var c=getContext("2d")
        c.reset(); c.scale(width/32,height/32)
        c.strokeStyle="white"; c.fillStyle="white"; c.lineWidth=1.5; c.lineCap="round"; c.lineJoin="round"
        c.beginPath(); c.moveTo(12,22); c.lineTo(12,20)
        c.bezierCurveTo(12,17,8,16,8,11)
        c.bezierCurveTo(8,1,24,1,24,11)
        c.bezierCurveTo(24,16,20,17,20,20)
        c.lineTo(20,22); c.closePath()
        if(lit) c.fill(); else c.stroke()
        c.beginPath(); c.moveTo(12,26); c.lineTo(20,26); c.moveTo(14,29); c.lineTo(18,29); c.stroke()
    }
}
