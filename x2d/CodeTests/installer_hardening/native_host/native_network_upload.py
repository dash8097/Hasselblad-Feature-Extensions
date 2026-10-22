"""经现有相机 USB 网卡上传引导或完整安装载荷；失败后不重发或回退写入。"""
import hashlib
import re
import socket
import time

LOCAL = '192.168.42.1'
CAMERA = '192.168.42.2'
PORT = 30306
STAGE = '/data/local/tmp/x2d-rs1'


def receiver_command(token):
    if token != 'native-install' and not re.fullmatch(r'z[0-9a-f]{8}', token):
        raise ValueError('network-upload-token')
    command = ("cd " + STAGE + " && (umask 077;nohup sh -c 'set -C;"
               "/bin/toybox timeout -k 1 30 /bin/toybox netcat -l -s " + CAMERA +
               ' -p 30306 sh -c "set -C;cat >' + token + '.gz";echo $? >' + token +
               ".rc' </dev/null >/dev/null 2>&1 &)")
    if len(command.encode('ascii')) > 231:
        raise ValueError('network-upload-command-limit')
    return command


def upload(installer, data, stage, token, digest):
    """返回 False 只表示本机无 USB 网卡，且尚未发送任何设备命令。"""
    if stage != STAGE or hashlib.sha256(data).hexdigest() != digest:
        raise ValueError('network-upload-input')
    command = receiver_command(token)
    connection = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        try:
            connection.bind((LOCAL, 0))
        except OSError:
            return False
        connection.settimeout(5)
        state = installer.file_state(stage, token + '.gz')
        if state == digest:
            return True
        if state != 'ABSENT' or installer.file_state(stage, token + '.rc') != 'ABSENT':
            raise RuntimeError('USB 网络上传存在未确认文件；保留现场，不覆盖或重发')
        if installer.one("if grep -q ':7662 .* 0A ' /proc/net/tcp;then echo BUSY;else echo FREE;fi") != 'FREE':
            raise RuntimeError('USB 网络接收端口被占用，未派发')
        installer.status('通过相机 USB 网络传输已校验载荷')
        installer.one(command)
        # 不重启接收器、不重发数据；只等待同一个监听端就绪。
        for attempt in range(10):
            try:
                connection.connect((CAMERA, PORT))
                break
            except ConnectionRefusedError:
                if attempt == 9:
                    raise
                connection.close()
                time.sleep(.1)
                connection = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                connection.bind((LOCAL, 0))
                connection.settimeout(5)
        for offset in range(0, len(data), 65536):
            connection.sendall(data[offset:offset + 65536])
            installer.progress('PATCH_PROGRESS=transfer|' + str(min(offset + 65536, len(data))) + '|' + str(len(data)))
        connection.shutdown(socket.SHUT_WR)
        while connection.recv(1024):
            raise RuntimeError('USB 网络接收器返回了非预期数据')
        for _ in range(20):
            reply = installer.one('cd ' + stage + ';if test -f ' + token + '.rc;then cat ' + token + '.rc;else echo WAIT;fi')
            if reply != 'WAIT':
                break
            time.sleep(.1)
        if reply != '0' or installer.file_state(stage, token + '.gz') != digest:
            raise RuntimeError('USB 网络传输完成校验失败；保留现场，不重发')
        installer.log.append('USB_NETWORK_UPLOAD_VERIFIED bytes=' + str(len(data)))
        return True
    finally:
        connection.close()
