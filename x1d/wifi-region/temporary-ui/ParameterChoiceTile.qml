import QtQuick 2.5

Item {
    id:root;width:100;height:80
    property alias source:icon.source
    property string text:""
    property color textColor:"white"
    property alias textWrapping:caption.wrapMode
    property bool showInvertedFocus:true
    property bool acceptKeys:true
    property color highlightColor:"#707070"
    property bool fitText:false
    signal clicked()
    signal pressed()
    Rectangle {anchors.fill:parent;color:root.showInvertedFocus && (root.focus || touch.pressed)?root.highlightColor:"transparent"}
    Image {id:icon;anchors.centerIn:parent;fillMode:Image.PreserveAspectFit}
    Text {
        id:caption;anchors.top:parent.bottom;anchors.topMargin:2;width:parent.width
        text:root.text;color:root.textColor;font.pixelSize:20
        font.weight:touch.pressed?Font.Bold:Font.Light
        horizontalAlignment:Text.AlignHCenter;wrapMode:Text.Wrap
        fontSizeMode:root.fitText?Text.HorizontalFit:Text.FixedSize
    }
    MouseArea {id:touch;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.bottom:caption.bottom;onPressed:root.pressed();onClicked:root.clicked()}
    Keys.onReturnPressed:if(acceptKeys)clicked();else event.accepted=false
}
