"""真实 Qt 执行计时、文件持久化和插行；平台代理不连接设备。"""
from pathlib import Path
import json, os, sys, shutil
sys.dont_write_bytecode = True
D = Path(__file__).resolve().parent
X = D.parents[1]
sys.path.insert(0, str(X/'outputs/4.2.0/face-edge-fix/deps'))
os.environ.update(QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software', QML_DISABLE_DISK_CACHE='1',
                  QML_XHR_ALLOW_FILE_READ='1', QML_XHR_ALLOW_FILE_WRITE='1')
from PySide6.QtCore import QUrl
from PySide6.QtGui import QGuiApplication
from PySide6.QtQml import QQmlEngine, QQmlComponent, QQmlExpression
from PySide6.QtTest import QTest
from prepare_liveview import build, OUT
build()
W = OUT/'qt-check'; W.mkdir(exist_ok=True)
for name in ['policy.qml','preferences.qml','model.qml']:
    if not (W/name).exists(): (W/name).write_text('',encoding='utf-8')
for name in ['Policy','Preferences','Model']:
    shutil.copyfile(D/f'X2dScreenLiveview{name}.qml', W/f'X2dScreenLiveview{name}.qml')
app=QGuiApplication([]); engine=QQmlEngine(); components=[]; checks=[]; warnings=[]
engine.warnings.connect(lambda ws: warnings.extend(w.toString() for w in ws))
def write(name,text): (W/name).write_text(text,encoding='utf-8')
def make(name):
    c=QQmlComponent(engine,QUrl.fromLocalFile(str(W/name)));components.append(c)
    assert not c.isError(), '\n'.join(e.toString() for e in c.errors())
    o=c.create();assert o, '\n'.join(e.toString() for e in c.errors());return o
def expr(o,code):
    e=QQmlExpression(engine.rootContext(),o,code);v=e.evaluate()
    assert not e.hasError(), e.error().toString()
    return v[0] if isinstance(v,tuple) else v
def ok(value,name):
    assert value,name
    checks.append(name)
write('policy.qml','''import QtQuick
X2dScreenLiveviewPolicy { property int hits:0;onExpired:hits++ }''')
p=make('policy.qml');ok(not p.property('waiting'),'默认不计时')
p.setProperty('eligible',True);ok(not p.property('waiting'),'关闭值不计时')
p.setProperty('seconds',1);QTest.qWait(550);expr(p,'activity()');QTest.qWait(550)
ok(p.property('hits')==0 and p.property('waiting'),'活动重置完整时长')
QTest.qWait(550);ok(p.property('hits')==1,'空闲一次到期')
QTest.qWait(1050);ok(p.property('hits')==1,'到期后不重复退出')
expr(p,'activity()');p.setProperty('touching',True);QTest.qWait(1050)
ok(p.property('hits')==1 and not p.property('waiting'),'持续触摸不退出')
p.setProperty('touching',False);QTest.qWait(1050);ok(p.property('hits')==2,'释放后重新计时')
expr(p,'activity()');p.setProperty('eligible',False);QTest.qWait(1050)
ok(p.property('hits')==2,'拍摄或页面不允许时取消')
p.setProperty('eligible',True);p.setProperty('seconds',0);QTest.qWait(1050)
ok(p.property('hits')==2,'运行中关闭立即取消')

config=W/'preferences.txt'
if config.exists():config.unlink()
write('preferences.qml','import QtQuick\nX2dScreenLiveviewPreferences { settingsUrl:'+json.dumps(QUrl.fromLocalFile(str(config)).toString())+';property int saves:0;onSaved:saves++ }')
prefs=make('preferences.qml');QTest.qWait(100)
ok(prefs.property('ready') and prefs.property('seconds')==0 and not config.exists(),'首次无文件可选择且不自动写盘')
ok(not expr(prefs,'select(15)') and not config.exists(),'拒绝未列出的秒数')
expr(prefs,'select(30)');QTest.qWait(100)
ok(config.read_bytes()==b'X2DLV1 30\n' and prefs.property('seconds')==30 and prefs.property('saves')==1,'真实文件 PUT 与 GET 回读后保存')
again=make('preferences.qml');QTest.qWait(100)
ok(again.property('seconds')==30,'重建后恢复持久化值')
config.write_text('bad data',encoding='utf-8');expr(again,'read(-1)');QTest.qWait(100)
ok(again.property('seconds')==0 and again.property('ready') and again.property('failed'),'损坏文件停用计时但保留恢复入口')
expr(again,'select(10)');QTest.qWait(100)
ok(config.read_bytes()==b'X2DLV1 10\n' and not again.property('failed'),'损坏后重新选择可恢复')
config.write_bytes(b'X2DLV1 30\n');expr(again,'finish(false,0);select(0)');QTest.qWait(100)
ok(config.read_bytes()==b'X2DLV1 0\n','失败后选择关闭必须覆盖磁盘旧正值')
reopened=make('preferences.qml');QTest.qWait(100)
ok(reopened.property('seconds')==0 and not reopened.property('failed'),'失败后关闭重启仍保持关闭')

write('model.qml','''import QtQuick
Item {
 id:r
 property alias adapted:adapted
 ListModel {
  id:stock
  function srcIndex(name){for(var i=0;i<count;i++)if(get(i).itemName===name)return i;return -1}
  function itemEnabled(index){return get(index).itemEnabled}
  function isSectionFirstInList(name){return name==="power"}
  function separatorTopPadding(name){return 4}
  function separatorBottomPadding(name){return 7}
  ListElement{itemName:"before";text1:"Before";text2:"";description:"";propValue:1;itemEnabled:true;elem:2;proxy:1;options:0;sectionName:"power";demo:false}
  ListElement{itemName:"screen_idle_timeout";text1:"Screen";text2:"";description:"";propValue:300;itemEnabled:true;elem:2;proxy:1;options:0;sectionName:"power";demo:false}
  ListElement{itemName:"after";text1:"After";text2:"";description:"";propValue:0;itemEnabled:false;elem:2;proxy:1;options:0;sectionName:"power";demo:false}
 }
 X2dScreenLiveviewModel {id:adapted;sourceModel:stock;label:"取景超时";seconds:30;available:false;stockDelegate:Component{Item{}}}
 function names(){var a=[];for(var i=0;i<adapted.items.count;i++)a.push(adapted.items.get(i).model.itemName);return a.join(',')}
 function row(i){return adapted.items.get(i).model.text1}
 function removeAnchor(){stock.remove(1)}
 function restoreAnchor(){stock.insert(1,{itemName:"screen_idle_timeout",text1:"Screen",text2:"",description:"",propValue:300,itemEnabled:true,elem:2,proxy:1,options:0,sectionName:"power",demo:false})}
 function stockCount(){return stock.count}
}''')
m=make('model.qml');QTest.qWait(100)
ok(expr(m,'names()')=='before,x2d_liveview_seconds,screen_idle_timeout,after','插行紧邻原厂熄屏时间上方')
ok(expr(m,'adapted.totalSize')==4 and expr(m,'stockCount()')==3,'总数正确且原模型不变')
ok(expr(m,'adapted.srcIndex("screen_idle_timeout")')==2 and not expr(m,'adapted.itemEnabled(3)'),'原厂索引与禁用项映射正确')
ok(not expr(m,'adapted.itemEnabled(1)'),'自定义委托就绪前禁用行')
expr(m,'adapted.available=true;adapted.label="Live view timeout";adapted.seconds=60');QTest.qWait(80)
ok(expr(m,'row(1)')=='Live view timeout' and expr(m,'adapted.items.get(1).model.propValue')==60,'语言与值更新不重复插行')
expr(m,'removeAnchor()');QTest.qWait(80);ok(expr(m,'names()')=='before,after','移除原厂锚点则移除附加行')
expr(m,'restoreAnchor()');QTest.qWait(80);ok(expr(m,'names()')=='before,x2d_liveview_seconds,screen_idle_timeout,after','模型恢复重新插入')
ok(not warnings,'Qt 无异常警告：'+ '\n'.join(warnings))
(OUT/'qt-validation.json').write_text(json.dumps({'checks':checks,'count':len(checks),'deviceAccess':False},ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print('PASS',len(checks),'checks; no device access')
