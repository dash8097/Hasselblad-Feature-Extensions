"""固定 4.2.0 原厂组件的 Qt 检查；仅替身硬件状态和确认按键区域。"""
def prepare(write,q):
    for name in ('ExposeScreen','ExposeIconText','ExposeProgress'):
        text=q[':/app/qml/exposescreen/'+name+'.qml'].decode()
        if name=='ExposeIconText':text=text.replace('id: textItem','id: textItem\n            objectName:"testFactoryCountdownText"')
        if name=='ExposeProgress':
            text=text.replace('id: textItem1','id: textItem1\n                objectName:"testFactoryProgressText"')
            text=text.replace('id: textItem2','id: textItem2\n                objectName:"testFactoryProgressCount"')
            text=text.replace('id: textItem3','id: textItem3\n        objectName:"testFactoryProgressDetail"')
        write('factory/exposescreen/'+name+'.qml',text)
    for name in ('ProgressBarRounded','ProgressShader','ScaledImage','ConfirmButtonConfiguration'):
        write('factory/components/'+name+'.qml',q[':/app/qml/components/'+name+'.qml'].decode())
    write('factory/components/UiImage.qml','import QtQuick\nImage {}')
    write('factory/components/StatusRow.qml','import QtQuick\nItem {\n'+
          '\n'.join('property bool '+n+':false' for n in ('alwaysShowBattery','showWifi','flashReady','flashCharging','batteryStatusValid','batteryWarning','batteryLevelCritical','charging','showTemperatureWarning'))+
          '\nproperty int batteryLevel:100\n}')
    write('factory/components/ExposuresAvailCounter.qml','import QtQuick\nText {property bool isTethered:false;property int exposuresLeft:0;property bool mediaAvailableOrActive:true}')
    write('factory/components/ConfirmButtonArea.qml','''import QtQuick
Item {
 objectName:"ConfirmButtonArea"
 property alias btn1:c1
 property alias btn2:c2
 property alias background:bg
 property bool sideOriented:false
 ConfirmButtonConfiguration{id:c1}
 ConfirmButtonConfiguration{id:c2}
 Rectangle{id:bg;width:parent.width;height:parent.height*0.2;anchors.bottom:parent.bottom}
}''')
    write('factory/viewmodels/ExposeScreenViewModel.qml','''import QtQuick
Item {
 property bool sideOriented:false
 property bool flashReady:false
 property bool flashCharging:false
 property bool batteryStatusValid:true
 property bool batteryWarning:false
 property bool batteryLevelCritical:false
 property bool charging:false
 property int batteryLevel:100
 property bool isTethered:false
 property int exposuresLeft:100
 function tdur(value){return String(value)}
}''')
    write('factory/scripts/Keys.js',q[':/app/qml/scripts/Keys.js'].decode())
