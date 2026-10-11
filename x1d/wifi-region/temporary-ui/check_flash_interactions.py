"""离线触控检查；不连接相机，不发送引闪命令。"""
from pathlib import Path
import os, sys
P = Path(__file__).resolve().parent
os.environ['QT_QPA_PLATFORM'] = 'offscreen'
os.environ['QT_QUICK_BACKEND'] = 'software'
sys.path.insert(0, str(P.parents[1]/'wireless-flash/build/ui-test-python'))
from PySide6.QtCore import QUrl, QPoint, QPointF, Qt, QObject
from PySide6.QtGui import QGuiApplication, QFontDatabase, QFont
from PySide6.QtQml import QQmlPropertyMap
from PySide6.QtQuick import QQuickView
from PySide6.QtTest import QTest
from PySide6.QtCore import QResource
assert QResource.registerResource(str(P/'build/flash-ui.rcc'))
app=QGuiApplication([])
QFontDatabase.addApplicationFont('C:/Windows/Fonts/msyh.ttc')
app.setFont(QFont('Microsoft YaHei'))
host=P/'build/host-flash'
(host/'FlashHeaderText.qml').write_bytes((P/'FlashHeaderText.qml').read_bytes())
(host/'FlashText.qml').write_bytes((P/'build/flash-source/FlashText.qml').read_bytes())
(host/'FlashStyle.qml').write_bytes((P/'build/flash-source/FlashStyle.qml').read_bytes())
(host/'FlashIconButton.qml').write_bytes((P/'build/flash-source/FlashIconButton.qml').read_bytes())
(host/'common/RadioModeButton.qml').write_bytes((P/'build/flash-source/RadioModeButton.qml').read_bytes())
(host/'SettingsToggle.qml').write_bytes((P/'SettingsToggle.qml').read_bytes())
source=(P/'build/flash-source/FlashPage.qml').read_text(encoding='utf-8')
source=source.replace('import "../settings/components"','import "selector"').replace('import "../common"','import "common"')
(host/'FlashPage.qml').write_text(source,encoding='utf-8')
constants=QQmlPropertyMap()
for k,v in dict(dragThreshold=20,swipeLengthDividor=5,menuSwipeDuration=200,fadeOutDuration=150,fadeOutOpacity=.6,numberOfItemsVisibleInList=5,listViewSizeIncreaseFactor=1,listSelectorYOffsetAscii=0,listSelectorYOffsetUnicode=0,menuItemFontName='Microsoft YaHei',highlightColor='white',highlightItemColor='white',itemColor='white',popoverListViewShadingStartColor='black',popupBackgroundColor='black',popupBorderColor='gray',popupFadeoutColor='black').items(): constants.insert(k,v)
view=QQuickView();view.rootContext().setContextProperty('constants',constants)
view.setSource(QUrl.fromLocalFile(str(host/'FlashPage.qml')))
assert view.status()==QQuickView.Ready, [e.toString() for e in view.errors()]
view.setResizeMode(QQuickView.SizeRootObjectToView);view.resize(640,480);view.show()
page=view.rootObject();page.setProperty('fontName','Microsoft YaHei')
def settle(): QTest.qWait(260)
def drag(x,y,dx,dy):
    QTest.mousePress(view,Qt.LeftButton,Qt.NoModifier,QPoint(x,y))
    for i in range(1,11): QTest.mouseMove(view,QPoint(x+dx*i//10,y+dy*i//10),15)
    QTest.mouseRelease(view,Qt.LeftButton,Qt.NoModifier,QPoint(x+dx,y+dy));settle()

# A release must leave the outgoing page visible until it reaches the edge.
# This catches the previous immediate screen switch even when final state passes.
for screen, name, start, end in [
    ('power','FlashPowerEditor',(300,220),(130,220)),
    ('selection','FlashGroupSelection',(240,465),(430,465)),
    ('settings','FlashSettingsPage',(240,460),(430,460)),
]:
    page.setProperty('screen',screen);settle()
    pane=page.findChild(QObject,name)
    QTest.mousePress(view,Qt.LeftButton,Qt.NoModifier,QPoint(*start))
    for i in range(1,11):
        QTest.mouseMove(view,QPoint(start[0]+(end[0]-start[0])*i//10,start[1]),15)
    released_x=pane.property('x')
    QTest.mouseRelease(view,Qt.LeftButton,Qt.NoModifier,QPoint(*end))
    assert page.property('screen')==screen,'release must not hide '+screen
    QTest.qWait(65)
    assert page.property('screen')==screen and pane.property('visible'),'mid-animation page stays visible '+screen
    assert abs(pane.property('x')-released_x)>5,'release must continue moving '+screen
    settle()
    assert page.property('screen')=='groups','close only after animation '+screen
page.setProperty('screen','power');settle()
base=page.findChild(QObject,'DetailPowerBase')
base_center=base.property('x')+base.property('width')/2
assert abs(base.mapToScene(QPointF(base.width()/2,base.height()/2)).x()-320)<=0.5,'base power must center on entire screen (pixel rounding)'
assert page.findChild(QObject,'FlashExposureSummary').property('visible'),'exposure header on detail'
QTest.mouseClick(view,Qt.LeftButton,Qt.NoModifier,QPoint(85,120));settle()
QTest.mouseClick(view,Qt.LeftButton,Qt.NoModifier,QPoint(580,340));settle()
assert page.findChild(QObject,'DetailPowerFraction').property('text'),'fraction appears'
assert abs(base.property('x')+base.property('width')/2-base_center)<0.1,'fraction must not move base power center'
header_before=view.grabWindow().copy(0,0,640,76)
QTest.mousePress(view,Qt.LeftButton,Qt.NoModifier,QPoint(310,220))
for i in range(1,9):QTest.mouseMove(view,QPoint(310,220-i*15),15)
QTest.qWait(40)
assert view.grabWindow().copy(0,0,640,76)==header_before,'detail swipe must not cover header'
def visual_items(item):
    yield item
    for child in item.childItems():yield from visual_items(child)
assert sum(bool(p.property('visible')) for p in visual_items(page) if p.objectName()=='FlashPowerEditor')>=2,'adjacent group preview'
QTest.mouseRelease(view,Qt.LeftButton,Qt.NoModifier,QPoint(310,100));settle()
page.setProperty('selectedGroup',3);settle()
for name in ['FlashTest','FlashSettings','FlashChooseGroups']:
    assert not page.findChild(QObject,name).property('visible'),name
before=page.property('selectedGroup');drag(310,220,0,-120)
assert page.property('selectedGroup')==before+1,'vertical switch'
drag(310,220,45,0);assert page.property('screen')=='power','short swipe'
drag(300,220,-170,0);assert page.property('screen')=='groups','left return'
page.setProperty('screen','selection');settle()
left_brackets=[p for p in visual_items(page) if p.objectName()=='SelectedGroupLeftBracket']
assert len(left_brackets)==16
assert sum(bool(p.property('visible')) for p in left_brackets)==page.property('visibleGroupCount'),'selected groups retain bracket markers'
drag(240,465,190,0);assert page.property('screen')=='groups','bottom blank return'
page.setProperty('screen','power');settle()
QTest.mouseClick(view,Qt.LeftButton,Qt.NoModifier,QPoint(85,120));settle()
assert page.property('currentGroupEnabled'),'enable offline group'
power_before=page.property('currentPower')
drag(200,340,190,0);assert page.property('screen')=='power','strip must not navigate'
assert page.property('currentPower')>power_before,'strip changes draft power'
view.grabWindow().save(str(host/'detail-candidate.png'))
page.setProperty('screen','groups');settle()
group_list=page.findChild(QObject,'FlashGroupList')
assert group_list.property('height')==314,'four rows fill header-to-footer region'
assert 'width: 640; height: 78.5' in source,'four complete full-width rows'
assert group_list.property('y')+group_list.property('height')==396,'no fractional-row blank below list'
assert page.findChild(QObject,'FlashChooseGroups').property('visible'),'group footer retained'
assert page.findChild(QObject,'FlashSettings').property('visible'),'settings footer retained'
for name in ['FlashTest','FlashChooseGroups','FlashSettings']:
    button=page.findChild(QObject,name)
    assert button.property('label')=='','footer icons only'
    assert button.property('height')==82,'footer height matches native pressed cell'
    assert button.property('width')==100,'footer width matches native minimum cell'
view.grabWindow().save(str(host/'overview-alignment.png'))
page.setProperty('screen','settings');settle()
rows={p.objectName():p for p in visual_items(page) if p.objectName().startswith('FlashSettingRow')}
assert len(rows)==6
for i in range(6):
    assert rows['FlashSettingRow'+str(i)].property('height')==(106 if i<2 else 80)
assert rows['FlashSettingRow4'].property('y')-rows['FlashSettingRow3'].property('y')==80,'sync and shutter use normal menu row spacing'
settings_list=page.findChild(QObject,'FlashSettingsPage')
power_before=page.property('sendPowerUpdates')
sync_before=page.property('sendFlashSync')
QTest.mouseClick(view,Qt.LeftButton,Qt.NoModifier,QPoint(570,126));settle()
assert page.property('sendPowerUpdates')==power_before,'preserve existing mandatory power-update policy'
assert page.property('sendFlashSync')==sync_before,'power switch must not change sync'
QTest.mouseClick(view,Qt.LeftButton,Qt.NoModifier,QPoint(570,232));settle()
assert page.property('sendFlashSync')!=sync_before,'sync switch click'
settings_list.setProperty('contentY',300);settle()
view.grabWindow().save(str(host/'settings-spacing.png'))
drag(240,460,190,0);assert page.property('screen')=='groups','scrolled settings bottom returns'
for screen in ['selection','power','wireless','settings']:
    page.setProperty('screen',screen);settle()
    shot=view.grabWindow()
    for x in [0,7,632,639]:
        for y in [162,238,314,389,396,450,478]:
            assert shot.pixelColor(x,y).lightness()<10,(screen,x,y,'underlying line leaked')
    shot.save(str(host/(screen+'-backdrop.png')))
print('PASS: QML load, footer hidden, vertical group switch, short rebound, left return, bottom blank return, strip navigation isolation, full-width child backdrops')
