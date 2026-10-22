import build_native_attachment as b,os,ctypes
ctypes.windll.kernel32.SetErrorMode(0x8003)
e=dict(os.environ,TEMP=str(b.OUT),TMP=str(b.OUT),ZIG_LOCAL_CACHE_DIR=str(b.OUT/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(b.OUT/'zig-global'),QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software')
e['PATH']=str(b.CXX.parent)+os.pathsep+str(b.SDK/'bin')+os.pathsep+e['PATH']
i=[f'-I{b.SDK/"include"}',f'-I{b.HERE}']+[f'-I{b.SDK/"include"/("Qt"+m)}' for m in ['Core','Gui','Qml','Quick','Network']]
l=[b.SDK/'lib'/('libQt6'+m+'.a') for m in ['Quick','Qml','Network','Gui','Core']]
b.run([b.CXX,'-std=c++17','-O1','-DQT_NO_DEBUG',*i,b.HERE/'effect_preferences.cpp',b.OUT/'moc_effect_preferences.cpp',b.HERE/'native_attachment.cpp',b.OUT/'moc_native_attachment.cpp',b.HERE/'test_qt_parent_lifetime.cpp',*l,'-o',b.OUT/'lifetime-probe.exe'],e)
print(b.run([b.OUT/'lifetime-probe.exe'],e))
