"""真实 Qt 6.4.1 file GET/PUT、模式应用、防重复及重载无触发；无相机调用。"""
import json,hashlib
import os
from pathlib import Path
import shutil
import sys

sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[2]
OUT=ROOT/'x2d/outputs/4.2.0/pixel-shift-rgb/pixel-overclock-ui-r1'
if os.environ.get('X2D_SIM_CASE_OUT'):
    OUT=Path(os.environ['X2D_SIM_CASE_OUT'])
HOST=OUT/'qt-check';HOST.mkdir(exist_ok=True)
CHANNEL=HOST/'channel';CHANNEL.mkdir(exist_ok=True)
sys.path.insert(0,str(ROOT/'x2d/outputs/4.2.0/temporary-wifi-button/qt-runtime'))
sys.path.insert(0,str(HERE.parent/'temporary_af_speed_probe'))
os.environ.update(QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',QML_DISABLE_DISK_CACHE='1',
                  QML_XHR_ALLOW_FILE_READ='1',QML_XHR_ALLOW_FILE_WRITE='1')
from PySide6.QtCore import QUrl,QPoint,Qt,QMetaObject,QObject
from PySide6.QtGui import QGuiApplication,QFont,QFontDatabase,QImage
from PySide6.QtQml import QQmlApplicationEngine
from PySide6.QtQuick import QQuickWindow,QQuickImageProvider
from PySide6.QtTest import QTest
from inspect_menu_resources import load_gui,resources
from processing_ui_fixture import prepare as prepare_processing

def write(name,text):
    p=HOST/name;p.parent.mkdir(parents=True,exist_ok=True)
    p.write_text(text,encoding='utf-8',newline='\n')
def singleton(module,name,body):
    write('imports/com/hasselblad/'+module+'/qmldir',f'module com.hasselblad.{module}\nsingleton {name} 1.0 {name}.qml\n')
    write('imports/com/hasselblad/'+module+'/'+name+'.qml','pragma Singleton\nimport QtQuick\nQtObject {\n'+body+'\n}')

def main():
    for name in ('PixelOverclockController.qml','PixelOverclockPage.qml'):
        text=(HERE/name).read_text(encoding='utf-8').replace('qrc:/app/qml/scripts/Keys.js','scripts/Keys.js')
        text=text.replace('qrc:/app/qml/exposescreen','factory/exposescreen')
        write(name,text)
    q=dict(resources(load_gui()));prepare_processing(write,q)
    write('scripts/Keys.js',q[':/app/qml/scripts/Keys.js'].decode())
    singleton('constants','Constants','''
property real scaleFactor:1
property real scaleFactorX:1
property real scaleFactorY:1
property real cameraViewRightMargin:20
property real cameraViewMarginY:20
property color menuHeaderColor:"white"
property color colorWhite:"white"
property color colorBlack:"black"
property color menuBackgroundColor:"#202020"
property string progressFragmentShaderName:""
''')
    singleton('types','HblmTypes','enum Value { E_CameraKeyOption_Override=1 }')
    singleton('keys','KeyFn','enum Function { Escape, Abort, JstkEscape, Select, PopAccept }\nfunction isFn(key,modifiers,fn){return key===Qt.Key_Escape && fn===KeyFn.Abort}\nfunction isUnblockableCustomButton(key,modifiers){return false}')
    status=dict(protocol=3,session='1234abcd',ack=0,busy=False,ready=True,heartbeat=1,stage='idle',album='',error='',mode_epoch=0,preparing=False,prepared=False)
    def put_status(**kw):
        status.update(kw);status['heartbeat']+=1
        (CHANNEL/'status.json').write_text(json.dumps(status),encoding='utf-8')
    request=CHANNEL/'request.txt'
    if request.exists():request.unlink()
    put_status()
    arm=CHANNEL/'arm.txt'
    bridge=CHANNEL/'shutter-status.json'
    def native_echo(value=''):
        bridge.write_text(json.dumps(dict(protocol=1,ready=True,arm=value,available=True)),encoding='utf-8')
    native_echo()
    arm.write_text('DISARMED\n',encoding='utf-8')
    write('Scene.qml','''import QtQuick
import QtQuick.Window
import "factory/viewmodels" as NativeModel
Window {
 id:win; width:1024; height:768; visible:true
 property alias controller:ctl
 property int format:2
 property int pageClosed:0
 function applyMode() { ctl.applyMode() }
 function disarmMode() {ctl.disarmMode()}
 function applyStack() {if(!ctl.applyStack(0,10,true))throw new Error("stack apply failed")}
 function applyIsoStack() {if(!ctl.applyStack(3,60,false,2,100))throw new Error("ISO stack apply failed")}
 function finishStack() {ctl.finishStack()}
 function chooseKeep() { if(!ctl.setOption("keepMode",1)) throw new Error("option selection failed") }
 function rejectInvalidOption() { if(ctl.setOption("frames",4) || ctl.setOption("frames",6) || ctl.setOption("saveFormat",1) || ctl.setOption("initialDelay",1) || ctl.setOption("keepMode",2)) throw new Error("invalid accepted") }
 QtObject { id:camera; property int image_format:win.format }
 PixelOverclockController {id:ctl; objectName:"testController";cameraProxy:camera; channelDirectory:"'''+QUrl.fromLocalFile(str(CHANNEL)).toString()+'''/"}
 NativeModel.ExposeScreenViewModel{id:factory}
 PixelOverclockPage {id:page;objectName:"testPage";anchors.fill:parent; controller:ctl;viewModel:factory;onClose:win.pageClosed++}
}''')
    app=QGuiApplication([]);fid=QFontDatabase.addApplicationFont('C:/Windows/Fonts/msyh.ttc')
    app.setFont(QFont(QFontDatabase.applicationFontFamilies(fid)[0]))
    for name in ('ic_drive_mode_storage_left_bracket','ic_drive_mode_storage_right_bracket','ic_drive_mode_self_timer_big'):
        path=HOST/'icons'/(name+'.svg');path.parent.mkdir(exist_ok=True);path.write_bytes(q[':/icons/'+name+'.svg'])
    class Icons(QQuickImageProvider):
        def __init__(self):super().__init__(QQuickImageProvider.Image)
        def requestImage(self,name,size,requestedSize):
            value=QImage(str(HOST/'icons'/(name+'.svg')))
            if not value.isNull():
                value=value.scaled(round(value.width()*requestedSize.width()/100),round(value.height()*requestedSize.height()/100),Qt.KeepAspectRatio,Qt.SmoothTransformation)
                size.setWidth(value.width());size.setHeight(value.height())
            return value
    warnings=[]
    def load():
        engine=QQmlApplicationEngine();engine.addImportPath(str(HOST/'imports'))
        engine.addImageProvider('svg',Icons())
        engine.warnings.connect(lambda errors:warnings.extend(str(e) for e in errors))
        engine.load(QUrl.fromLocalFile(str(HOST/'Scene.qml')));assert engine.rootObjects(),warnings
        win=engine.rootObjects()[0];QTest.qWait(350);return engine,win,win.findChild(QObject,'testController')
    engine,win,ctl=load();assert ctl.property('canStart');assert not request.exists()
    for setting,raw,heif in [(0,1,False),(1,1,False),(2,0,False),(3,1,True),(4,0,True),(-1,-1,False)]:
        win.setProperty('format',setting);QTest.qWait(20)
        assert ctl.property('saveFormat')==raw and ctl.property('saveHeif')==heif
    QMetaObject.invokeMethod(win,'applyMode');QTest.qWait(100)
    assert arm.read_text()=='DISARMED\n' and not request.exists()
    win.setProperty('format',2)
    QMetaObject.invokeMethod(win,'applyMode');QTest.qWait(300)
    assert arm.read_text()=='PO3 1234abcd 1 6 2 0 START END\n'
    assert not ctl.property('appliedConfirmed') and not request.exists()
    native_echo(arm.read_text().strip());QTest.qWait(300)
    assert not ctl.property('appliedConfirmed') # arm 回显不等于真正准备完成。
    put_status(mode_epoch=1,stage='prepare',preparing=True,ready=False);QTest.qWait(350)
    assert ctl.property('preparingMode') and not request.exists()
    put_status(stage='idle',preparing=False,prepared=True,ready=True);QTest.qWait(350)
    assert ctl.property('appliedConfirmed') and not request.exists()
    # 未拍摄的准备失败也必须清缓存；同一请求无需切换单拍即可重试。
    put_status(stage='failed',prepared=False,error='准备失败');native_echo('DISARMED');QTest.qWait(350)
    assert not ctl.property('applied') and arm.read_text()=='DISARMED\n'
    QMetaObject.invokeMethod(win,'applyMode');QTest.qWait(350)
    assert ctl.property('preparingMode') and ctl.property('applied')
    assert arm.read_text()=='PO3 1234abcd 1 6 2 0 START END\n'
    assert ctl.property('stage')=='prepare' # 上一次失败状态不能取消这次新应用。
    put_status(mode_epoch=2,stage='idle',prepared=True,error='')
    native_echo(arm.read_text().strip());QTest.qWait(350)
    assert ctl.property('appliedConfirmed') and not request.exists()
    # 替身实体快门兑现 arm；此处明确不是 Qt 调用曝光。
    request.write_text(arm.read_text(),encoding='utf-8')
    put_status(ack=1,busy=True,stage='delay',remaining=2);QTest.qWait(350)
    countdown=win.findChild(QObject,'PixelOverclockNativeCountdown')
    progress=win.findChild(QObject,'PixelOverclockNativeProgress')
    assert countdown.property('visible') and not progress.property('visible')
    assert ctl.property('countdownRemaining')==2
    countdownText=win.findChild(QObject,'testFactoryCountdownText')
    progressText=win.findChild(QObject,'testFactoryProgressText')
    progressDetail=win.findChild(QObject,'testFactoryProgressDetail')
    assert countdownText.property('text')=='2'
    assert countdownText.property('font').pixelSize()==160
    put_status(stage='delay',remaining=1);QTest.qWait(350)
    assert countdownText.property('text')=='1'
    put_status(ack=1,busy=True,stage='capture',frames=6,delay=2,keep=0,format=0);QTest.qWait(350)
    assert not countdown.property('visible') and progress.property('visible')
    assert not progress.property('showInfiniteLoop')
    assert progressDetail.property('text')=='请保持相机稳定'
    assert progressText.property('font').pixelSize()==96
    assert progressDetail.property('font').pixelSize()==80
    assert not ctl.property('canStart')
    before=arm.read_text();QMetaObject.invokeMethod(win,'applyMode');QTest.qWait(100)
    assert arm.read_text()==before
    put_status(stage='render',render_tiles=3,work_done=5,work_total=17);QTest.qWait(350)
    assert progressText.property('text')=='显影'
    assert win.findChild(QObject,'testFactoryProgressCount').property('text')=='3/9'
    assert progressDetail.property('text')=='已完成 5/17 步'
    assert abs(progress.property('progressValue')-5/17)<0.001
    put_status(stage='render',render_tiles=9,work_done=12,work_total=17);QTest.qWait(350)
    assert progressText.property('text')=='生成 JPEG'
    assert progressDetail.property('text')=='已完成 12/17 步'
    assert abs(progress.property('progressValue')-12/17)<0.001
    # 原厂屏遮罩会吃掉触摸；忙时拒绝，失败恢复空闲后点击返回必须关闭。
    QTest.mouseClick(win,Qt.LeftButton,Qt.NoModifier,QPoint(100,700));QTest.qWait(50)
    assert win.property('pageClosed')==0
    put_status(busy=False,stage='failed',error='处理失败，原片已保留');QTest.qWait(350)
    touch=win.findChild(QObject,'PixelOverclockReturnTouch')
    assert touch.property('enabled')
    QTest.mouseClick(win,Qt.LeftButton,Qt.NoModifier,QPoint(100,700));QTest.qWait(50)
    assert win.property('pageClosed')==1
    page=win.findChild(QObject,'testPage')
    QMetaObject.invokeMethod(page,'abortPressed');QTest.qWait(50)
    assert win.property('pageClosed')==2
    put_status(busy=False,stage='complete',album='900HASBL/B0090000.JPG');QTest.qWait(300)
    assert arm.read_text()=='DISARMED\n' and not ctl.property('applied')
    assert ctl.property('canStart')
    assert win.grabWindow().save(str(OUT/'pixel-overclock-page-preview.png'))
    win.close();engine.deleteLater();QTest.qWait(100)
    native_echo()
    engine,win,ctl=load();assert request.read_text()=='PO3 1234abcd 1 6 2 0 START END\n'
    QTest.qWait(3500);assert not ctl.property('canStart')
    put_status(session='abcd1234',ack=0,stage='idle',mode_epoch=0,prepared=False);QTest.qWait(350);assert ctl.property('canStart')
    QMetaObject.invokeMethod(win,'chooseKeep');QMetaObject.invokeMethod(win,'rejectInvalidOption')
    win.setProperty('format',3)
    QMetaObject.invokeMethod(win,'applyMode');QTest.qWait(350)
    assert arm.read_text()=='PO3 abcd1234 1 6 2 1 START END\n'
    native_echo(arm.read_text().strip());QTest.qWait(350)
    put_status(mode_epoch=1,prepared=True,preparing=False,stage='idle');QTest.qWait(350)
    assert ctl.property('appliedConfirmed')
    request.write_text(arm.read_text(),encoding='utf-8')
    put_status(ack=1,busy=True,stage='capture',frames=6,delay=5,keep=1,format=1);QTest.qWait(350)
    win.close();engine.deleteLater();QTest.qWait(100)
    engine,win,ctl=load();win.setProperty('format',3)
    assert ctl.property('frames')==6 and ctl.property('initialDelay')==5 and ctl.property('keepMode') and ctl.property('saveFormat')==1
    assert request.read_text()=='PO3 abcd1234 1 6 2 1 START END\n'
    put_status(busy=False,stage='complete');QTest.qWait(350)
    assert arm.read_text()=='PO3 abcd1234 2 6 5 1 START END\n'
    assert request.read_text()=='PO3 abcd1234 1 6 2 1 START END\n' # 保持模式不自动拍摄。
    QMetaObject.invokeMethod(win,'disarmMode');QTest.qWait(350)
    assert arm.read_text()=='DISARMED\n'
    assert not warnings,warnings
    # 真正堆栈协议：准备不拍摄，完成只发结束请求，忙时禁止更改参数。
    put_status(busy=False,stage='idle',prepared=False,preparing=False,mode_epoch=2,stack=False)
    QTest.qWait(350)
    before_request=request.read_text()
    QMetaObject.invokeMethod(win,'applyStack');QTest.qWait(350)
    assert arm.read_text()=='RS4 abcd1234 2 0 10 1 2 START END\n'
    assert request.read_text()==before_request
    native_echo(arm.read_text().strip());QTest.qWait(350)
    put_status(mode_epoch=3,prepared=True,preparing=False,stage='idle');QTest.qWait(350)
    assert ctl.property('appliedConfirmed')
    put_status(ack=2,busy=True,stage='delay',stack=True,stack_frames=0,stack_gap_ms=0,stack_remaining=10,remaining=2,keep=0)
    QTest.qWait(350)
    assert win.findChild(QObject,'PixelOverclockNativeCountdown').property('visible') and ctl.property('stackFrameCount')==0
    assert not win.findChild(QObject,'testPage').property('finishingAllowed')
    put_status(ack=2,busy=True,stage='capture',stack=True,stack_frames=7,stack_gap_ms=210,stack_remaining=4,keep=0)
    QTest.qWait(350)
    assert ctl.property('stackFrameCount')==7 and ctl.property('stackGapMs')==210
    assert win.findChild(QObject,'testFactoryProgressText').property('text')=='4 秒'
    assert win.findChild(QObject,'testFactoryProgressCount').property('text')=='7 帧'
    detail=win.findChild(QObject,'testFactoryProgressDetail')
    assert detail.property('text')=='帧间隔 0.21 秒'
    assert detail.property('width')<=win.width()
    put_status(stack_remaining=0);QTest.qWait(350)
    assert win.findChild(QObject,'testFactoryProgressText').property('text')=='收尾中'
    assert detail.property('text')=='帧间隔 0.21 秒'
    before_arm=arm.read_text();ctl.setProperty('stackSeconds',20);QTest.qWait(100)
    assert arm.read_text()==before_arm
    QTest.mouseClick(win,Qt.LeftButton,Qt.NoModifier,QPoint(100,700));QTest.qWait(150)
    assert (CHANNEL/'finish.txt').read_text()=='RSF1 abcd1234 2 FINISH END\n'
    (CHANNEL/'finish.txt').unlink()
    win.findChild(QObject,'testPage').forceActiveFocus()
    QTest.keyClick(win,Qt.Key_Escape);QTest.qWait(150)
    assert (CHANNEL/'finish.txt').read_text()=='RSF1 abcd1234 2 FINISH END\n'
    assert request.read_text()==before_request
    put_status(busy=False,stage='complete',album='900HASBL/B0090001.JPG',stack_frames=8,stack_gap_ms=250,stack_remaining=0);QTest.qWait(350)
    assert not ctl.property('applied') and arm.read_text()=='DISARMED\n'
    assert ctl.property('stackFrameCount')==8 and ctl.property('stackGapMs')==250
    put_status(stage='idle',stack=False,prepared=False,mode_epoch=3);QTest.qWait(350)
    QMetaObject.invokeMethod(win,'applyIsoStack');QTest.qWait(350)
    assert arm.read_text()=='RS5 abcd1234 3 3 100 0 2 START END\n'
    assert request.read_text()==before_request
    native_echo(arm.read_text().strip())
    put_status(mode_epoch=4,prepared=True);QTest.qWait(350)
    assert ctl.property('appliedConfirmed')
    put_status(ack=3,busy=True,stage='capture',stack=True,stack_iso_mode=True,
               stack_source_iso=1600,stack_target_iso=100,stack_planned_frames=16,
               stack_shutter_n=1,stack_shutter_d=200,stack_frames=8)
    QTest.qWait(350)
    assert ctl.property('stackIsoMode') and ctl.property('stackPlannedFrames')==16
    assert ctl.property('stackOutputIso')==200 and abs(ctl.property('stackOutputSeconds')-.04)<1e-9
    assert ctl.property('stackOutputShutterText')=='1/25 秒'
    assert '计划 16 帧' in detail.property('text') and 'ISO 200.00' in detail.property('text'),detail.property('text')
    (CHANNEL/'finish.txt').unlink()
    QMetaObject.invokeMethod(win,'finishStack');QTest.qWait(150)
    assert (CHANNEL/'finish.txt').read_text()=='RSF1 abcd1234 3 FINISH END\n'
    put_status(busy=False,stage='complete',stack_frames=8);QTest.qWait(350)
    assert ctl.property('stackOutputIso')==200 and not ctl.property('applied')
    assert not warnings,warnings
    win.close()
    (OUT/'qt-check.json').write_text(json.dumps(dict(qt='6.4.1',real_local_file_get_put=True,
         files={n:hashlib.sha256((HERE/n).read_bytes()).hexdigest() for n in ('PixelOverclockController.qml','PixelOverclockPage.qml')},
         physical_request_mocked=True,apply_without_capture=True,native_quality_formats_verified=True,busy_disabled=True,reload_does_not_capture=True,
         stale_backend_disabled=True,device_verified=False),indent=2)+'\n')
    print('PIXEL_OVERCLOCK_QT_FILE_CHANNEL_REQUEST_RELOAD_AND_STALE_CHECK_PASSED')

if __name__=='__main__':main()
