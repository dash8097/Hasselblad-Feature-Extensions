"""X2D 4.2.0 vendor interface 3，协议与冻结 Windows 客户端一致。

导入不加载 libusb、不枚举设备。只在显式操作时打开；不读取序列号，
不 detach/set_configuration/reset/clear_halt，不使用存在生命周期缺陷的 ID81。
"""
import ctypes as C
import math
import re
import secrets
import struct
import time
from pathlib import Path

class UsbError(RuntimeError):
    pass

def crc(data):
    value = 0
    for byte in data:
        value ^= byte << 8
        for _ in range(8):
            value = ((value << 1) ^ (0x1021 if value & 0x8000 else 0)) & 0xffff
    return value

def shell_request(command, tag):
    try:
        text = command.encode('ascii')
    except UnicodeEncodeError:
        raise UsbError('SHELL_ASCII_ONLY') from None
    if not 1 <= len(text) <= 231 or any(x < 32 or x > 126 for x in text):
        raise UsbError('SHELL_REQUEST_LIMIT')
    p = bytearray(257)
    p[:5] = bytes([10, 0, 8, 5, 252])
    struct.pack_into('<I', p, 5, 65)
    struct.pack_into('<I', p, 13, tag)
    p[25:25+len(text)] = text
    struct.pack_into('<I', p, 17, crc(p[21:257]))
    return bytes(p)

def shell_frame(p, tag):
    if len(p) not in (260, 1024) or p[:5] != bytes([9, 0, 5, 8, 252]):
        raise UsbError('SHELL_FRAME')
    msg, function, received, check, status = struct.unpack_from('<IIIII', p, 5)
    if msg != 65 or received != tag:
        raise UsbError('SHELL_HEADER')
    if check != crc(p[21:257]):
        raise UsbError('SHELL_CRC')
    if function not in (0, 4):
        raise UsbError('SHELL_REPLY_TYPE')
    if status:
        raise UsbError('SHELL_REMOTE_FAILURE')
    return function

def reviewed_value(ident, value):
    if not 1 <= len(value) <= 100 or any(x < 32 or x > 126 for x in value):
        return False
    text = value.decode('ascii')
    if ident == 27:
        m = re.fullmatch(r'S([2-9]|[1-9]\d),(v?\d{1,3}(?:\.\d{1,3}){1,4})', text)
        return bool(m and int(m[1]) == len(m[2]))
    if ident == 25:
        return bool(re.fullmatch(r'i(?:0|[1-9]\d{0,3})', text) and int(text[1:]) <= 3600)
    if ident == 87:
        if not re.fullmatch(r'f(?:0|[1-9a-f][0-9a-f]{0,15})', text):
            return False
        ev = struct.unpack('<d', struct.pack('<Q', int(text[1:], 16)))[0]
        return math.isfinite(ev) and abs(ev) <= 100 and abs(ev*12-round(ev*12)) < 1e-8
    return ident in (28, 88, 61) and text in ('i0', 'i1')

def parameter_frame(p):
    if not 8 <= len(p) <= 1024 or p[:4] != bytes([3, 0, 5, 8]) or not p[4]:
        raise UsbError('USB_REPLY_LIMIT')
    if len(p) < 5+p[4] or p[4] < 3 or p[7] != 0x82:
        raise UsbError('USB_FRAME_LENGTH')
    sequence = int.from_bytes(p[5:7], 'little')
    if not sequence:
        raise UsbError('USB_REPLY_SEQUENCE')
    return sequence, p[8:5+p[4]]

def control_pipes(order):
    if len(order) != 4 or len(set(order)) != 4:
        raise UsbError('USB_PIPE_MISMATCH')
    for i, ep in enumerate(order):
        if not ep & 15 or ep & 0x70 or bool(ep & 0x80) != (i >= 2):
            raise UsbError('USB_PIPE_MISMATCH')
    return order[3], order[1]

class Protocol:
    """可注入离线 wire；一条请求只发送一次，过滤回复有数量和时间上限。"""
    def __init__(self, wire):
        self.wire = wire

    def matching(self, parameter=False, sequence=0, tag=0, deadline=None, stale=None):
        deadline = time.monotonic()+45 if deadline is None else deadline
        stale = [0] if stale is None else stale
        while True:
            remaining = int((deadline-time.monotonic())*1000)
            if remaining <= 0:
                raise UsbError('USB_MATCH_DEADLINE')
            p = self.wire.read(remaining)
            if time.monotonic() >= deadline:
                raise UsbError('USB_MATCH_DEADLINE')
            if p[:4] == bytes([3, 0, 5, 8]):
                seq, value = parameter_frame(p)
                if parameter and seq == sequence:
                    return p
                if not any(reviewed_value(i, value) for i in (27, 28, 25, 87, 88, 61)):
                    raise UsbError('USB_VALUE_UNREVIEWED')
            elif p[:5] == bytes([9, 0, 5, 8, 252]):
                if len(p) not in (260, 1024):
                    raise UsbError('SHELL_FRAME')
                received = struct.unpack_from('<I', p, 13)[0]
                if not parameter and received == tag:
                    return p
                shell_frame(p, received)
            else:
                raise UsbError('USB_REPLY_ROUTE')
            stale[0] += 1
            if stale[0] > 16:
                raise UsbError('USB_STALE_REPLY_LIMIT')

    def read_parameter(self, ident, sequence):
        if ident not in (27, 28, 25, 87, 88, 61) or not 1 <= sequence <= 65535:
            raise UsbError('USB_REQUEST_DENIED')
        self.wire.write(bytes([4, 0, 8, 5, 5, sequence & 255, sequence >> 8, 2, ident, 0]))
        _, value = parameter_frame(self.matching(parameter=True, sequence=sequence))
        if not reviewed_value(ident, value):
            raise UsbError('USB_VALUE_UNREVIEWED')
        return value.decode('ascii')

    def handshake(self):
        sequence = secrets.randbelow(64999)+1
        if self.read_parameter(27, sequence) not in ('S5,4.2.0', 'S6,v4.2.0'):
            raise UsbError('FIRMWARE_MISMATCH：仅支持第一代 X2D 固件 4.2.0')
        if self.read_parameter(28, sequence+1) != 'i1':
            raise UsbError('CAMERA_NOT_RUNNING')

    def shell(self, command, tag):
        self.wire.write(shell_request(command, tag))
        output = bytearray()
        deferred = False
        # 全部回复共用 45 秒截止时间，包括多段通知。
        deadline = time.monotonic()+45
        stale = [0]
        for _ in range(64):
            p = self.matching(tag=tag, deadline=deadline, stale=stale)
            if time.monotonic() >= deadline:
                raise UsbError('USB_MATCH_DEADLINE')
            function = shell_frame(p, tag)
            if function == 0:
                if deferred:
                    raise UsbError('SHELL_OUTPUT_CHAR')
                return output.decode('ascii')
            data = p[25:257].split(b'\0', 1)[0]
            if len(output)+len(data) > 8192:
                raise UsbError('SHELL_OUTPUT_LIMIT')
            if any(x not in (9, 10, 13) and not 32 <= x <= 126 for x in data):
                deferred = True
            if not deferred:
                output.extend(data)
        raise UsbError('SHELL_FRAME_LIMIT')

    def batch(self, commands):
        if not 1 <= len(commands) <= 100:
            raise UsbError('BATCH_LIMIT')
        for command in commands:
            shell_request(command, 1)
        self.handshake()
        tag = secrets.randbelow(0x7fffffff-200)+1
        output = []
        for i, command in enumerate(commands):
            try:
                output.append(self.shell(command, tag+i))
            except UsbError as e:
                raise UsbError('BATCH_STEP_'+str(i)+'_'+str(e)) from None
            time.sleep(.05)
        return output

class DeviceDescriptor(C.Structure):
    _fields_ = [('length', C.c_uint8), ('type', C.c_uint8), ('usb', C.c_uint16),
        ('cls', C.c_uint8), ('sub', C.c_uint8), ('protocol', C.c_uint8), ('packet', C.c_uint8),
        ('vid', C.c_uint16), ('pid', C.c_uint16), ('device', C.c_uint16),
        ('manufacturer', C.c_uint8), ('product', C.c_uint8), ('serial', C.c_uint8), ('configs', C.c_uint8)]
class Endpoint(C.Structure):
    _fields_ = [('length', C.c_uint8), ('type', C.c_uint8), ('address', C.c_uint8),
        ('attributes', C.c_uint8), ('packet', C.c_uint16), ('interval', C.c_uint8),
        ('refresh', C.c_uint8), ('synch', C.c_uint8), ('extra', C.c_void_p), ('extra_len', C.c_int)]
class InterfaceDescriptor(C.Structure):
    _fields_ = [(n, C.c_uint8) for n in ('length','type','number','alt','count','cls','sub','protocol','string')]+[
        ('endpoints', C.POINTER(Endpoint)), ('extra', C.c_void_p), ('extra_len', C.c_int)]
class Interface(C.Structure):
    _fields_ = [('alts', C.POINTER(InterfaceDescriptor)), ('count', C.c_int)]
class Config(C.Structure):
    _fields_ = [('length', C.c_uint8), ('type', C.c_uint8), ('total', C.c_uint16),
        ('count', C.c_uint8), ('value', C.c_uint8), ('string', C.c_uint8), ('attrs', C.c_uint8),
        ('power', C.c_uint8), ('interfaces', C.POINTER(Interface)), ('extra', C.c_void_p), ('extra_len', C.c_int)]

class LibusbWire:
    def __init__(self, library):
        self.lib = C.CDLL(str(Path(library).resolve()))
        self.context = C.c_void_p(); self.handle = C.c_void_p(); self.claimed = False
        ptr = C.c_void_p
        signatures = {
            'init': (C.c_int, [C.POINTER(ptr)]), 'exit': (None, [ptr]),
            'get_device_list': (C.c_ssize_t, [ptr, C.POINTER(C.POINTER(ptr))]),
            'free_device_list': (None, [C.POINTER(ptr), C.c_int]),
            'get_device_descriptor': (C.c_int, [ptr, C.POINTER(DeviceDescriptor)]),
            'get_active_config_descriptor': (C.c_int, [ptr, C.POINTER(C.POINTER(Config))]),
            'free_config_descriptor': (None, [C.POINTER(Config)]),
            'open': (C.c_int, [ptr, C.POINTER(ptr)]), 'close': (None, [ptr]),
            'get_configuration': (C.c_int, [ptr, C.POINTER(C.c_int)]),
            'kernel_driver_active': (C.c_int, [ptr, C.c_int]),
            'claim_interface': (C.c_int, [ptr, C.c_int]), 'release_interface': (C.c_int, [ptr, C.c_int]),
            'bulk_transfer': (C.c_int, [ptr, C.c_uint8, ptr, C.c_int, C.POINTER(C.c_int), C.c_uint])}
        for name, (result, args) in signatures.items():
            fn = getattr(self.lib, 'libusb_'+name); fn.restype = result; fn.argtypes = args

    @staticmethod
    def check(result, stage):
        if result < 0:
            raise UsbError(stage+'（libusb '+str(result)+'）。请关闭 Phocus 并检查 USB 连接。')

    def __enter__(self):
        try:
            self.check(self.lib.libusb_init(C.byref(self.context)), 'USB_INIT')
            devices = C.POINTER(C.c_void_p)()
            count = self.lib.libusb_get_device_list(self.context, C.byref(devices))
            self.check(count, 'USB_ENUMERATE')
            try:
                matches = []
                for i in range(count):
                    d = DeviceDescriptor()
                    self.check(self.lib.libusb_get_device_descriptor(devices[i], C.byref(d)), 'USB_DESCRIPTOR')
                    if (d.vid, d.pid) == (0x2756, 0x0009):
                        matches.append(devices[i])
                if len(matches) != 1:
                    raise UsbError('请仅连接一台第一代 X2D；当前匹配数量='+str(len(matches)))
                config = C.POINTER(Config)()
                self.check(self.lib.libusb_get_active_config_descriptor(matches[0], C.byref(config)), 'USB_ACTIVE_CONFIG')
                try:
                    candidates = []
                    for i in range(config.contents.count):
                        interface = config.contents.interfaces[i]
                        for j in range(interface.count):
                            d = interface.alts[j]
                            if (d.number,d.alt,d.cls,d.sub,d.protocol,d.count) == (3,0,255,0,0,4):
                                if any(d.endpoints[k].attributes & 3 != 2 for k in range(4)):
                                    raise UsbError('USB_PIPE_MISMATCH')
                                candidates.append(control_pipes([d.endpoints[k].address for k in range(4)]))
                    if len(candidates) != 1:
                        raise UsbError('USB_INTERFACE_MISMATCH')
                    config_value = config.contents.value
                    self.pipe_in, self.pipe_out = candidates[0]
                finally:
                    self.lib.libusb_free_config_descriptor(config)
                self.check(self.lib.libusb_open(matches[0], C.byref(self.handle)), 'USB_OPEN')
                active = C.c_int()
                self.check(self.lib.libusb_get_configuration(self.handle, C.byref(active)), 'USB_ACTIVE_CONFIG')
                if not active.value or active.value != config_value:
                    raise UsbError('USB_CONFIGURATION_CHANGED；未设置 USB 配置')
                driver = self.lib.libusb_kernel_driver_active(self.handle, 3)
                if driver == 1:
                    raise UsbError('USB_INTERFACE_BUSY；请退出 Phocus，未卸载或接管驱动')
                if driver not in (0, -12):
                    self.check(driver, 'USB_DRIVER_CHECK')
                self.check(self.lib.libusb_claim_interface(self.handle, 3), 'USB_CLAIM')
                self.claimed = True
            finally:
                self.lib.libusb_free_device_list(devices, 1)
            return self
        except BaseException:
            self.close()
            raise

    def write(self, data):
        buffer = C.create_string_buffer(data); transferred = C.c_int()
        self.check(self.lib.libusb_bulk_transfer(self.handle,self.pipe_out,buffer,len(data),C.byref(transferred),45000), 'USB_WRITE')
        if transferred.value != len(data):
            raise UsbError('USB_SHORT_WRITE；未重发命令')

    def read(self, timeout):
        buffer = C.create_string_buffer(1024); transferred = C.c_int()
        self.check(self.lib.libusb_bulk_transfer(self.handle,self.pipe_in,buffer,1024,C.byref(transferred),timeout), 'USB_READ')
        return buffer.raw[:transferred.value]

    def close(self):
        error = 0
        if self.handle:
            if self.claimed:
                error = self.lib.libusb_release_interface(self.handle, 3)
            self.claimed = False
            self.lib.libusb_close(self.handle); self.handle = C.c_void_p()
        if self.context:
            self.lib.libusb_exit(self.context); self.context = C.c_void_p()
        return error

    def __exit__(self, kind, value, tb):
        error = self.close()
        if kind is None:
            self.check(error, 'USB_CLOSE')

def batch(library, commands):
    # 在打开设备之前验证所有命令长度。
    for command in commands:
        shell_request(command, 1)
    if not 1 <= len(commands) <= 100:
        raise UsbError('BATCH_LIMIT')
    with LibusbWire(library) as wire:
        return Protocol(wire).batch(commands)
