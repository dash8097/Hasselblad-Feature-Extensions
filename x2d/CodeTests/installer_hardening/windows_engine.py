"""Windows 编译型安装编排入口。构建为 pyd；源码只留内部，导入不访问设备。"""
import base64
import ctypes
import json
import re
import subprocess
import sys
from pathlib import Path
import mac_patch


class Transport:
    def __init__(self, root):
        self.root=Path(root)
        self.library=None

    def call(self,operation,commands=()):
        if self.library is None:
            lib=ctypes.CDLL(str(self.root/'X2dTransport.dll'))
            lib.x2d_call.argtypes=(ctypes.c_void_p,ctypes.c_int)
            lib.x2d_call.restype=ctypes.c_void_p
            lib.x2d_free.argtypes=(ctypes.c_void_p,)
            lib.x2d_free.restype=None
            self.library=lib
        request=json.dumps(dict(operation=operation,commands=list(commands)),ensure_ascii=False).encode('utf-8')
        if len(request)>1048576:raise ValueError('ABI_REQUEST_LIMIT')
        buffer=ctypes.create_string_buffer(request)
        reply=self.library.x2d_call(buffer,len(request))
        if not reply:raise RuntimeError('ABI_EMPTY_REPLY')
        try:
            pointer=ctypes.cast(reply,ctypes.POINTER(ctypes.c_ubyte))
            length=0
            while length<33554432 and pointer[length]:length+=1
            if length==33554432:raise RuntimeError('ABI_REPLY_LIMIT')
            result=json.loads(ctypes.string_at(reply,length).decode('utf-8'))
        finally:self.library.x2d_free(reply)
        if result['error']:raise RuntimeError(result['error']+'; WIN32='+str(result['win32']))
        return result['values']

    def batch(self,commands):
        return self.call('batch',commands)

    def diagnose_batch(self,commands):
        values=self.call('fault',commands)
        if len(values)!=len(commands)+1 or values[0] not in ('i0','i1'):
            raise RuntimeError('FAULT_REPLY_SHAPE')
        return values[1:]


def emit_context(prefix,context,emit):
    value=dict(context,protocol=1)
    emit(prefix+base64.b64encode(json.dumps(value,ensure_ascii=False).encode()).decode('ascii'))


def preflight(root,emit):
    result=subprocess.run([str(Path(root)/'X2dHostCheck.exe')],capture_output=True,timeout=30,
                          creationflags=subprocess.CREATE_NO_WINDOW,encoding='utf-8')
    snapshot=json.loads(result.stdout.lstrip('\ufeff'))
    s=snapshot.get('snapshot')
    if s:
        emit('PHOCUS_INSTALLED_VERSION='+','.join(s['Versions']))
        emit('PHOCUS_RUNNING='+str(s['Running'])+'; DRIVER_PACKAGES='+str(s['DriverPackages'])+
             '; WINUSB='+str(s['WinUsbAvailable'])+'; BINDING_ISSUES='+str(s['BindingIssues']))
    if result.returncode or snapshot.get('error'):raise RuntimeError(snapshot.get('error') or '主机预检失败')
    emit('PHOCUS_NOT_RUNNING_AND_X2D_WINUSB_DRIVER_PACKAGE_VERIFIED')


def run_request(root,request,emit=print,precheck=None,transport=None):
    """私有测试可注入传输；命令行不提供注入、任意 shell 或测试开关。"""
    action=request.get('action')
    if action not in ('Install','ResumeUpdate','Uninstall','Diagnose'):raise ValueError('ACTION_DENIED')
    mask=request.get('feature_mask',248)
    if type(mask) is not int or mask not in (248,255):raise ValueError('FEATURE_MASK_DENIED')
    pending=request.get('pending','')
    if action=='ResumeUpdate':
        if not isinstance(pending,str) or not re.fullmatch('[a-f0-9]{64}',pending):raise ValueError('PENDING_INVALID')
    elif pending:raise ValueError('PENDING_UNEXPECTED')
    wire=Transport(root)
    engine=mac_patch.Installer(root,run_batch=transport or (wire.diagnose_batch if action=='Diagnose' else wire.batch),progress=emit)
    engine.feature_mask=mask
    engine.native_usb_network_upload = transport is None
    if pending:engine.pending=pending;engine.pending_mask=mask
    try:
        # 与 USB 路径解耦；电脑预检失败时完全不打开相机。
        (precheck or preflight)(root,emit)
        message=engine.run('Install' if action=='ResumeUpdate' else action)
        engine.context['action']=action
        engine.context['host_exit']='0'
        if action=='Diagnose':
            emit_context('PATCH_DIAGNOSTIC=',engine.context,emit)
        elif engine.pending:
            if pending:emit('PATCH_UPDATE_RESTART_STILL_PENDING')
            else:emit('PATCH_UPDATE_RESTART_PENDING='+engine.pending)
        elif action=='Uninstall':
            emit('PATCH_ALREADY_UNINSTALLED' if engine.context.get('route')=='Stock' else 'PATCH_UNINSTALLED_MANUAL_REBOOT_REQUIRED')
        elif engine.context.get('route')=='CurrentStock':emit('PATCH_ALREADY_CURRENT')
        else:emit('PATCH_MANUAL_REBOOT_REQUIRED')
        emit_context('PATCH_CONTEXT=',engine.context,emit)
        emit(mac_patch.protected(message))
        for line in engine.log:emit(mac_patch.protected(line))
        return 0
    except Exception as exc:
        engine.context.update(action=action,host_exit='1')
        if not engine.context.get('reason'):engine.context['reason']=mac_patch.protected(exc)
        if engine.first_failure:engine.context['first_failure']=engine.first_failure
        emit_context('PATCH_DIAGNOSTIC=',engine.context,emit)
        for line in engine.log:emit(mac_patch.protected(line))
        return 1


def main():
    # 原生宿主设置隔离模块路径；只接受固定三种用户操作及重启续接。
    if len(sys.argv)!=2:raise SystemExit(2)
    root=Path(sys.argv[1]).resolve()
    raw=sys.stdin.buffer.readline(4097)
    if not raw or len(raw)>4096:raise SystemExit(2)
    try:
        request=json.loads(raw.decode('utf-8'))
        if not isinstance(request,dict) or set(request)-{'action','feature_mask','pending'}:raise ValueError('REQUEST_INVALID')
        rc=run_request(root,request,lambda value:print(value,flush=True))
    except Exception:
        print('NATIVE_ENGINE_REQUEST_REJECTED',flush=True);rc=2
    raise SystemExit(rc)
