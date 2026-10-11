import QtQuick 2.5

Rectangle {
    property bool checked: false
    property bool available: true
    width:86;height:36;radius:18
    color:checked?"#e85c49":"black"
    border.width:2;border.color:available?"#bbb":"#555"
    Rectangle {
        x:parent.checked?parent.width-width-3:3
        y:3;width:30;height:30;radius:15
        color:parent.available?"white":"#777"
    }
}
