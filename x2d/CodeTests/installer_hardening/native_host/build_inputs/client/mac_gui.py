"""原生 AppKit 三按钮窗口。界面启动不枚举 USB；点击后在独立线程操作。

不依赖 Tk、Electron、系统 Python、Homebrew 或浏览器。
"""
import json
import ctypes as C
import platform
import queue
import sys
import threading
import time
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from mac_patch import Installer, protected

BUTTONS = ('检测并安装', '卸载', '诊断')
ACTIONS = ('Install', 'Uninstall', 'Diagnose')
GITHUB = 'https://github.com/dash8097/Hasselblad-Feature-Extensions'

class Point(C.Structure):
    _fields_ = [('x', C.c_double), ('y', C.c_double)]
class Size(C.Structure):
    _fields_ = [('w', C.c_double), ('h', C.c_double)]
class Rect(C.Structure):
    _fields_ = [('origin', Point), ('size', Size)]

class App:
    def __init__(self):
        if sys.platform != 'darwin':
            raise RuntimeError('此界面仅支持 macOS')
        C.CDLL('/System/Library/Frameworks/AppKit.framework/AppKit')
        self.objc = C.CDLL('/usr/lib/libobjc.A.dylib')
        self.objc.objc_getClass.restype = C.c_void_p
        self.objc.objc_getClass.argtypes = [C.c_char_p]
        self.objc.sel_registerName.restype = C.c_void_p
        self.objc.sel_registerName.argtypes = [C.c_char_p]
        self.objc.objc_allocateClassPair.restype = C.c_void_p
        self.objc.objc_allocateClassPair.argtypes = [C.c_void_p,C.c_char_p,C.c_size_t]
        self.objc.class_addMethod.restype = C.c_bool
        self.objc.class_addMethod.argtypes = [C.c_void_p,C.c_void_p,C.c_void_p,C.c_char_p]
        self.objc.objc_registerClassPair.argtypes = [C.c_void_p]
        self.callbacks = []; self.functions = {}; self.busy = False; self.events = queue.Queue()
        self.diagnostic = 0; self.diagnostic_view = 0
        self.refresh = 0
        root = Path(__file__).resolve().parent
        architecture = 'arm64' if platform.machine() == 'arm64' else 'x86_64'
        self.installer = Installer(root,root/'usb'/architecture/'libusb-1.0.dylib',progress=lambda text: self.events.put(('status',text)))
        self.pool = self.send(self.cls('NSAutoreleasePool'),'new')
        delegate_class = self.objc.objc_allocateClassPair(self.cls('NSObject'), b'X2DPatchDelegate', 0)
        self.add_method(delegate_class, 'pressed:', self.pressed, None, b'v@:@')
        self.add_method(delegate_class, 'tick:', self.tick, None, b'v@:@')
        self.add_method(delegate_class, 'copy:', self.copy_diagnostics, None, b'v@:@')
        self.add_method(delegate_class, 'github:', self.open_github, None, b'v@:@')
        self.add_method(delegate_class, 'windowShouldClose:', self.should_close, C.c_bool, b'B@:@')
        self.objc.objc_registerClassPair(delegate_class)
        self.delegate = self.send(delegate_class,'new')
        self.application = self.send(self.cls('NSApplication'),'sharedApplication')
        self.send(self.application,'setActivationPolicy:',None,(C.c_long,0))
        self.window = self.window_new('哈苏特调补丁 · X2D',720,624)
        self.send(self.window,'setDelegate:',None,(C.c_void_p,self.delegate))
        content = self.send(self.window,'contentView')
        self.label(content,'H',28,28,56,56,30,bold=True)
        self.label(content,'哈苏特调补丁',100,25,540,40,24,bold=True)
        self.label(content,'X2D 100C  /  固件 4.2.0  /  R5.8.52',102,65,540,22,12,muted=True)
        self.label(content,'完整功能测试包；下列功能统一安装。',28,113,624,22,12,muted=True)
        self.feature_indicators=[]
        catalog=json.loads((root/'feature-catalog.json').read_text(encoding='utf-8'))
        for i,feature in enumerate(catalog['features']):
            box=self.make('NSButton',Rect(Point(28+(i%2)*334,624-154-(i//2)*36-30),Size(326,30)))
            self.send(box,'setButtonType:',None,(C.c_ulong,3))
            self.send(box,'setTitle:',None,(C.c_void_p,self.string(feature['label'])))
            self.send(box,'setState:',None,(C.c_long,int(feature['default'])))
            self.send(content,'addSubview:',None,(C.c_void_p,box))
            self.send(box,'setEnabled:',None,(C.c_bool,False))
            self.feature_indicators.append((box,feature['bit']))
        self.label(content,'四亿、十六亿、RAW 堆栈及 AFC 开放；不包含 AFC 记录工具。',28,304,664,22,11,muted=True)
        self.label(content,'连接相机，然后选择操作',40,338,634,30,16,bold=True)
        self.state = self.label(content,'等待操作。请关闭 Phocus，并通过 USB 连接相机。',40,378,634,48,13,muted=True)
        self.progress = self.make('NSProgressIndicator',Rect(Point(40,178),Size(634,12)))
        self.send(self.progress,'setMinValue:',None,(C.c_double,0))
        self.send(self.progress,'setMaxValue:',None,(C.c_double,100))
        self.send(self.progress,'setIndeterminate:',None,(C.c_bool,False))
        self.send(content,'addSubview:',None,(C.c_void_p,self.progress))
        self.elapsed = self.label(content,'',40,450,634,20,11,muted=True)
        self.started = 0
        self.buttons = []
        for i, (title, x, width) in enumerate(zip(BUTTONS,(28,302,484),(256,164,168))):
            button = self.make('NSButton',Rect(Point(x,96),Size(width,42)))
            self.send(button,'setTitle:',None,(C.c_void_p,self.string(title)))
            self.send(button,'setBezelStyle:',None,(C.c_ulong,1))
            self.send(button,'setTag:',None,(C.c_long,i))
            self.send(button,'setTarget:',None,(C.c_void_p,self.delegate))
            self.send(button,'setAction:',None,(C.c_void_p,self.selector('pressed:')))
            if i == 0:
                self.send(button,'setKeyEquivalent:',None,(C.c_void_p,self.string('\r')))
            self.send(content,'addSubview:',None,(C.c_void_p,button))
            self.buttons.append(button)
        self.label(content,'其他来源的补丁请先通过官方固件还原。完成后按提示手动重启。',28,540,664,22,11,muted=True)
        github = self.make('NSButton',Rect(Point(28,32),Size(624,24)))
        self.send(github,'setTitle:',None,(C.c_void_p,self.string('公开 GitHub：'+GITHUB)))
        self.send(github,'setBordered:',None,(C.c_bool,False))
        self.send(github,'setTarget:',None,(C.c_void_p,self.delegate))
        self.send(github,'setAction:',None,(C.c_void_p,self.selector('github:')))
        self.send(content,'addSubview:',None,(C.c_void_p,github))
        self.label(content,'交流群入口在 GitHub 页面。日志在诊断中查看。',28,599,664,22,11,muted=True)
        self.timer = self.send(self.cls('NSTimer'),'scheduledTimerWithTimeInterval:target:selector:userInfo:repeats:',C.c_void_p,
            (C.c_double,.1),(C.c_void_p,self.delegate),(C.c_void_p,self.selector('tick:')),(C.c_void_p,0),(C.c_bool,True))

    def cls(self, name):
        return self.objc.objc_getClass(name.encode('ascii'))

    def selector(self, name):
        return self.objc.sel_registerName(name.encode('ascii'))

    def send(self, receiver, selector, result=C.c_void_p, *args):
        signature = (result, tuple(t for t,_ in args))
        if signature not in self.functions:
            self.functions[signature] = C.CFUNCTYPE(result,C.c_void_p,C.c_void_p,*signature[1])( ('objc_msgSend',self.objc))
        return self.functions[signature](receiver,self.selector(selector),*(v for _,v in args))

    def string(self, text):
        return self.send(self.cls('NSString'),'stringWithUTF8String:',C.c_void_p,(C.c_char_p,text.encode('utf-8')))

    def make(self, name, rect):
        return self.send(self.send(self.cls(name),'alloc'),'initWithFrame:',C.c_void_p,(Rect,rect))

    def add_method(self, cls, name, method, result, encoding):
        # callbacks 必须保持强引用，否则 Objective-C 持有的函数指针会失效。
        def guarded(receiver, selector, sender):
            try:
                return method(sender)
            except Exception as exc:
                self.events.put(('error','界面更新未完成：'+protected(exc)))
                return False if result is C.c_bool else None
        callback = C.CFUNCTYPE(result,C.c_void_p,C.c_void_p,C.c_void_p)(guarded)
        self.callbacks.append(callback)
        if not self.objc.class_addMethod(cls,self.selector(name),C.cast(callback,C.c_void_p),encoding):
            raise RuntimeError('原生界面回调注册失败')

    def window_new(self, title, width, height):
        window = self.send(self.send(self.cls('NSWindow'),'alloc'),'initWithContentRect:styleMask:backing:defer:',C.c_void_p,
            (Rect,Rect(Point(0,0),Size(width,height))),(C.c_ulong,1|2|4),(C.c_ulong,2),(C.c_bool,False))
        self.send(window,'setTitle:',None,(C.c_void_p,self.string(title)))
        self.send(window,'setReleasedWhenClosed:',None,(C.c_bool,False))
        self.send(window,'center',None)
        return window

    def label(self, content, text, x, y, width, height, size, bold=False, muted=False):
        field = self.make('NSTextField',Rect(Point(x,624-y-height),Size(width,height)))
        for selector, value in [('setEditable:',False),('setSelectable:',False),('setBezeled:',False),('setDrawsBackground:',False)]:
            self.send(field,selector,None,(C.c_bool,value))
        font = self.send(self.cls('NSFont'),'boldSystemFontOfSize:' if bold else 'systemFontOfSize:',C.c_void_p,(C.c_double,size))
        self.send(field,'setFont:',None,(C.c_void_p,font))
        if muted:
            self.send(field,'setTextColor:',None,(C.c_void_p,self.send(self.cls('NSColor'),'secondaryLabelColor')))
        self.send(field,'setStringValue:',None,(C.c_void_p,self.string(text)))
        self.send(content,'addSubview:',None,(C.c_void_p,field))
        return field

    def pressed(self, sender):
        if self.busy:
            return
        tag = self.send(sender,'tag',C.c_long)
        if not 0 <= tag < 3:
            return
        self.installer.feature_mask=255
        for box,bit in self.feature_indicators:self.send(box,'setEnabled:',None,(C.c_bool,False))
        self.busy = True
        self.started = time.monotonic()
        self.progress_percent = 0
        self.send(self.progress,'setIndeterminate:',None,(C.c_bool,False))
        self.send(self.progress,'setDoubleValue:',None,(C.c_double,0))
        self.send(self.state,'setStringValue:',None,(C.c_void_p,self.string('正在检查；请保持 USB 连接。')))
        for button in self.buttons:
            self.send(button,'setEnabled:',None,(C.c_bool,False))
        if self.refresh:
            self.send(self.refresh,'setEnabled:',None,(C.c_bool,False))
        action = ACTIONS[tag]
        def worker():
            try:
                result = self.installer.run(action)
                self.events.put(('complete',(action,result)))
            except Exception as exc:
                self.events.put(('failed',protected(exc)))
        threading.Thread(target=worker,daemon=True).start()

    def tick(self, sender):
        # AppKit 只在主线程更新；后台线程仅写队列。
        if self.busy:
            self.send(self.elapsed,'setStringValue:',None,(C.c_void_p,self.string('总进度 '+str(getattr(self,'progress_percent',0))+'% · 已用时 '+str(int(time.monotonic()-self.started))+' 秒 · 请保持 USB 连接')))
        while True:
            try:
                kind, data = self.events.get_nowait()
            except queue.Empty:
                return
            if kind == 'status':
                parsed = progress_value(data)
                if parsed is not None:
                    self.progress_percent = max(getattr(self,'progress_percent',0),parsed)
                    self.send(self.progress,'setDoubleValue:',None,(C.c_double,self.progress_percent))
                    self.send(self.state,'setStringValue:',None,(C.c_void_p,self.string('总进度 '+str(self.progress_percent)+'% · '+progress_detail(data))))
                elif not data.startswith('PATCH_PROGRESS='):
                    self.send(self.state,'setStringValue:',None,(C.c_void_p,self.string('总进度 '+str(getattr(self,'progress_percent',0))+'% · '+data)))
            elif kind in ('complete','failed'):
                self.busy = False
                for box,bit in getattr(self,'feature_indicators',[]):self.send(box,'setEnabled:',None,(C.c_bool,False))
                self.send(self.progress,'stopAnimation:',None,(C.c_void_p,0))
                self.send(self.progress,'setIndeterminate:',None,(C.c_bool,False))
                self.progress_percent = 100 if kind=='complete' else getattr(self,'progress_percent',0)
                self.send(self.progress,'setDoubleValue:',None,(C.c_double,self.progress_percent))
                self.send(self.elapsed,'setStringValue:',None,(C.c_void_p,self.string('总进度 '+str(self.progress_percent)+'% · 本次操作用时 '+str(int(time.monotonic()-self.started))+' 秒')))
                for button in self.buttons:
                    self.send(button,'setEnabled:',None,(C.c_bool,True))
                if self.refresh:
                    self.send(self.refresh,'setEnabled:',None,(C.c_bool,True))
                text = data[1] if kind == 'complete' else '操作未确认，请查看诊断。'+data
                self.send(self.state,'setStringValue:',None,(C.c_void_p,self.string(text)))
                if kind == 'failed' or data[0] == 'Diagnose':
                    try:
                        self.show_diagnostics()
                    except Exception:
                        # 显示诊断失败不会把已经完成的文件事务改报为安装失败。
                        self.send(self.state,'setStringValue:',None,(C.c_void_p,self.string(text+'\n诊断界面未能打开。')))
            elif kind == 'error':
                self.send(self.state,'setStringValue:',None,(C.c_void_p,self.string(data)))

    def show_diagnostics(self):
        if not self.diagnostic:
            self.diagnostic = self.window_new('X2D · 诊断',740,520)
            content = self.send(self.diagnostic,'contentView')
            scroll = self.make('NSScrollView',Rect(Point(20,66),Size(700,434)))
            self.send(scroll,'setHasVerticalScroller:',None,(C.c_bool,True))
            self.diagnostic_view = self.make('NSTextView',Rect(Point(0,0),Size(680,480)))
            self.send(self.diagnostic_view,'setEditable:',None,(C.c_bool,False))
            self.send(self.diagnostic_view,'setVerticallyResizable:',None,(C.c_bool,True))
            self.send(self.diagnostic_view,'setHorizontallyResizable:',None,(C.c_bool,False))
            self.send(self.diagnostic_view,'setAutoresizingMask:',None,(C.c_ulong,2))
            font = self.send(self.cls('NSFont'),'systemFontOfSize:',C.c_void_p,(C.c_double,12))
            self.send(self.diagnostic_view,'setFont:',None,(C.c_void_p,font))
            container = self.send(self.diagnostic_view,'textContainer')
            self.send(container,'setWidthTracksTextView:',None,(C.c_bool,True))
            self.send(scroll,'setDocumentView:',None,(C.c_void_p,self.diagnostic_view))
            self.send(content,'addSubview:',None,(C.c_void_p,scroll))
            for title, x, selector, tag in [('复制完整结果',20,'copy:',3),('重新读取现场（不安装）',200,'pressed:',2)]:
                button = self.make('NSButton',Rect(Point(x,18),Size(260 if tag == 2 else 160,32)))
                self.send(button,'setTitle:',None,(C.c_void_p,self.string(title)))
                self.send(button,'setBezelStyle:',None,(C.c_ulong,1))
                self.send(button,'setTag:',None,(C.c_long,tag))
                self.send(button,'setTarget:',None,(C.c_void_p,self.delegate))
                self.send(button,'setAction:',None,(C.c_void_p,self.selector(selector)))
                self.send(content,'addSubview:',None,(C.c_void_p,button))
                if tag == 2:
                    self.refresh = button
        self.send(self.diagnostic_view,'setString:',None,(C.c_void_p,self.string(self.installer.diagnostic_text())))
        self.send(self.diagnostic,'makeKeyAndOrderFront:',None,(C.c_void_p,0))

    def copy_diagnostics(self, sender):
        pasteboard = self.send(self.cls('NSPasteboard'),'generalPasteboard')
        self.send(pasteboard,'clearContents',C.c_long)
        self.send(pasteboard,'setString:forType:',C.c_bool,(C.c_void_p,self.string(self.installer.diagnostic_text())),
                  (C.c_void_p,self.string('public.utf8-plain-text')))

    def open_github(self, sender):
        # 只在用户点击后打开公开页面，不启动更新检查或 USB 会话。
        workspace = self.send(self.cls('NSWorkspace'),'sharedWorkspace')
        url = self.send(self.cls('NSURL'),'URLWithString:',C.c_void_p,(C.c_void_p,self.string(GITHUB)))
        self.send(workspace,'openURL:',C.c_bool,(C.c_void_p,url))

    def should_close(self, sender):
        if self.busy:
            self.send(self.state,'setStringValue:',None,(C.c_void_p,self.string('正在处理，请等待本次事务结束再关闭。')))
            return False
        self.send(self.application,'terminate:',None,(C.c_void_p,0))
        return True

    def run(self):
        self.send(self.window,'makeKeyAndOrderFront:',None,(C.c_void_p,0))
        self.send(self.application,'activateIgnoringOtherApps:',None,(C.c_bool,True))
        self.send(self.application,'run',None)

def progress_value(text):
    if not text.startswith('PATCH_PROGRESS='):
        return None
    fields=text[15:].split('|')
    ranges={'preflight':(0,5),'snapshot':(5,15),'transfer':(15,75),'install':(75,90),'verify':(90,99),'diagnose':(0,99),'cleanup-pre':(15,20),'cleanup-post':(99,99)}
    if len(fields)!=3 or fields[0] not in ranges:
        return None
    try:
        done,total=int(fields[1]),int(fields[2])
    except ValueError:
        return None
    if not 0<=done<=total<=9223372036854775807 or total==0:return None
    start,end=ranges[fields[0]]
    return start+done*(end-start)//total

def progress_detail(text):
    fields=text[15:].split('|');done,total=int(fields[1]),int(fields[2])
    title={'preflight':'检查安装包','snapshot':'检查相机组件','transfer':'传输载荷','install':'安装／恢复文件','verify':'核对完成文件','diagnose':'采集诊断','cleanup-pre':'回收旧暂存','cleanup-post':'清理安装收尾'}[fields[0]]
    units=str(done//1024)+' / '+str(total//1024)+' KB' if fields[0]=='transfer' else str(done)+' / '+str(total)
    return title+' '+str(done*100//total)+'% · '+units

if __name__ == '__main__':
    try:
        App().run()
    except Exception as exc:
        # 启动失败也提供可读结果；此时没有任何按钮操作，未打开 USB。
        objc = C.CDLL('/usr/lib/libobjc.A.dylib')
        C.CDLL('/System/Library/Frameworks/AppKit.framework/AppKit')
        objc.objc_getClass.restype = C.c_void_p; objc.objc_getClass.argtypes = [C.c_char_p]
        objc.sel_registerName.restype = C.c_void_p; objc.sel_registerName.argtypes = [C.c_char_p]
        send = C.CFUNCTYPE(C.c_void_p,C.c_void_p,C.c_void_p)(('objc_msgSend',objc))
        arg = C.CFUNCTYPE(C.c_void_p,C.c_void_p,C.c_void_p,C.c_void_p)(('objc_msgSend',objc))
        string = C.CFUNCTYPE(C.c_void_p,C.c_void_p,C.c_void_p,C.c_char_p)(('objc_msgSend',objc))
        alert = send(objc.objc_getClass(b'NSAlert'),objc.sel_registerName(b'new'))
        title = string(objc.objc_getClass(b'NSString'),objc.sel_registerName(b'stringWithUTF8String:'),'安装客户端未能启动；未连接相机。'.encode('utf-8'))
        detail = string(objc.objc_getClass(b'NSString'),objc.sel_registerName(b'stringWithUTF8String:'),protected(exc).encode('utf-8'))
        arg(alert,objc.sel_registerName(b'setMessageText:'),title)
        arg(alert,objc.sel_registerName(b'setInformativeText:'),detail)
        send(alert,objc.sel_registerName(b'runModal'))
        raise SystemExit(1)
