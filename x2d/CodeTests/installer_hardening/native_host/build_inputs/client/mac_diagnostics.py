"""完整诊断字段；命令清单由冻结 Windows PatchDiagnostics.ps1 离线导出。

错误服务只保存门控及编号，worker 只保存门控、阶段和错误，任务控制日志
逐段解码与脱敏。既不打开照片，也不把 UID/METADATA 等私有信息写入报告。
"""
import base64
import json
import re

TASK_SELECTION_COMMAND = "ls -td /mnt/media_rw/ssd/multishot/native-album-ui-* 2>/dev/null|head -n 1|sed 's|.*/||'; true"

def pin_task(items, reply):
    job = reply.strip() or 'ABSENT'
    if job != 'ABSENT' and not re.fullmatch(r'native-album-ui-[0-9a-f]{8}-[1-9][0-9]{0,9}',job):
        raise ValueError('任务编号格式未确认')
    result = []
    for original in items:
        item = dict(original)
        if item['name'].startswith('task/'):
            # 冻结命令模板只有占位任务目录可替换，不执行任意路径。
            template = 'j=/mnt/media_rw/ssd/multishot/ABSENT;'
            if not item['command'].startswith(template):
                raise ValueError('任务诊断模板版本不匹配')
            item['command'] = item['command'].replace(template,'j=/mnt/media_rw/ssd/multishot/'+job+';',1)
            item['task_id'] = job
            if len(item['command'].encode('ascii')) > 231:
                raise ValueError('诊断命令超出 USB 长度限制')
        result.append(item)
    return result

def decode(reply, limit):
    if len(reply) > limit or not re.fullmatch(r'[A-Za-z0-9+/=\r\n]+', reply.strip()):
        raise ValueError('运输格式未确认')
    return base64.b64decode(re.sub(r'\s', '', reply), validate=True).decode('utf-8', errors='strict')

def native_error(reply):
    try:
        reply = decode(reply,16384)
    except ValueError:
        # 只从原始 D-Bus 文本提取白名单字段，不显示整段错误字典。
        pass
    values = re.findall(r'string "active_error"\s+variant\s+boolean (true|false)\s*\)', reply)
    active = values[0] if len(values) == 1 else '未取得'
    fields = {}
    for key in ('CODE','CAT','ACTION'):
        values = re.findall(r'dict entry\(\s*string "'+key+r'"\s+variant\s+(?:int32|uint32) ([0-9]+)\s*\)', reply)
        fields[key] = values[0] if len(values) == 1 and int(values[0]) <= 2147483647 else '未取得'
    text = '原厂错误标志='+active+'；'+'；'.join(k+'='+v for k,v in fields.items())
    if active == 'true' and (fields['CODE'],fields['CAT'],fields['ACTION']) == ('40','5','1'):
        text += '；请跳过大容量存储。请在相机上操作，保持 USB 连接，再重新检查。'
    elif active == 'true':
        text += '；原厂有活动错误，请先核对相机提示；安装器不会自动确认或清除。'
    if active == '未取得':
        text += '；错误服务回复未能完整解析。'
    return text

def worker_summary(reply, protect):
    try:
        if re.fullmatch(r'[A-Za-z0-9+/=\r\n]+', reply.strip()):
            reply = decode(reply,8192)
        obj = json.loads(reply)
        fields = [key+'='+str(obj[key]).lower() for key in ('ready','busy','preparing','prepared') if type(obj.get(key)) is bool]
        if not fields:
            return '未取得有效 worker 状态'
        if isinstance(obj.get('stage'),str) and re.fullmatch('[a-z_]{1,40}',obj['stage']):
            fields.append('阶段='+obj['stage'])
        if isinstance(obj.get('error'),str) and len(obj['error']) <= 256:
            fields.append('后台错误='+protect(obj['error']))
        return '；'.join(fields)
    except (ValueError,TypeError,KeyError):
        return '未取得有效 worker 状态'

def task_log(reply, protect):
    if not reply.strip() or reply.strip() == 'ABSENT':
        return '日志不存在或不可读'
    try:
        return protect(decode(reply,8192))
    except ValueError:
        return '任务日志运输格式未确认'

def property_reply(reply, protect):
    if re.fullmatch(r'[A-Za-z0-9+/=\r\n]+',reply.strip()):
        try:
            return protect(decode(reply,8192))
        except ValueError:
            return '属性回复运输格式未确认'
    return protect(reply)

def convert(items, replies, protect):
    if len(items) != len(replies):
        raise ValueError('诊断回复数量不完整')
    data = {}
    raw = []
    for item, reply in zip(items,replies):
        name = item['name']
        value = (native_error(reply) if name == 'native_error' else
                 worker_summary(reply,protect) if name == 'worker' else
                 property_reply(reply,protect) if name in ('exposure','storage') else
                 task_log(reply,protect) if name.startswith(('task/','archive/')) or name == 'service_failures' else protect(reply))
        data[name] = value
        raw.append('['+name+'] '+protect(item['command'])+'\n'+value)
    job = next((i.get('task_id','未取得') for i in items if i['name'] == 'task/task.info'),'未取得')
    summary = ['固定任务编号：'+job+'。以下文件来自同一任务；当前状态及服务历史摘录独立标注。']
    for name in ('task.info','task.result','cleanup.result','failure.result','stage','run.rc','capture.rc','restored','memory.failed','task.events','task.memory','phase-events','phase-metrics','run.log','capture.log','merge.log','render.log','probe.log','finish-restore.log','watchdog.log','finish-metrics'):
        summary.append('任务 '+name+'：\n'+data['task/'+name])
        spec = next((i for i in items if i['name'] == 'task/'+name),{})
        if spec.get('tail_limit',0) >= 2048:
            summary.append('读取范围：末尾最多 '+str(spec['tail_limit'])+' 字节；更早内容可能未包含。')
    for path in ('/data','/data/local','/data/local/tmp','/data/local/tmp/x2d-rs1','/data/local/tmp/x2d-rs8'):
        summary.append('安装目录 '+path+'：'+data['directory'+path])
    for kind in ('stock','upgrade'):
        summary.append(kind+' 暂存区：result='+data[kind+'/result']+'；done='+data[kind+'/done'])
        summary.append(kind+' 安装日志（最多末尾20行）：\n'+data[kind+'/install.log'])
    summary.append('当前系统内存和服务 PID（诊断时读取）：\n'+data['current_memory'])
    for name in ('last-task.txt','last-failed-task.txt','last-cleanup.result'):
        summary.append('有界历史摘要 '+name+'（以摘要内 job_id 关联，不自动归属当前任务）：\n'+data['archive/'+name])
    summary.append('服务异常摘录（历史日志，未自动绑定上述任务）：\n'+data['service_failures'])
    for key in ('exposure','storage'):
        summary.append('原厂属性读取 '+key+'（读取失败不代表状态 0）：\n'+data[key])
    summary.append('任务存储目录（仅读取类型和访问状态，不创建目录）：\nSSD 根目录：'+data['job_root/mnt/media_rw/ssd']+
                   '\nMultiShot 父目录：'+data['job_root/mnt/media_rw/ssd/multishot']+'\nSSD 挂载：'+data['job_mount'])
    state = '任务锁='+data['lock']+'；菜单='+data['menu']+'\n'+data['worker']+'\n'+data['native_error']
    ui = re.search(r'rawStackUI=\d+ popup=\d+',data['ui'])
    state += '\nUI='+(ui[0] if ui else '未取得')
    for key in ('exposure','storage'):
        m = re.search(r'variant\s+(?:int32|uint32)\s+([0-9]+)\s*$',data[key])
        state += '；'+key+'='+(m[1] if m else '未取得')
    mount = data['mount'].strip()
    return dict(state=state, readonly='已读取：只读' if re.search(r'(^|,)ro(,|$)',mount) else '已读取：可写' if re.search(r'(^|,)rw(,|$)',mount) else '未取得',
                camera_exit='stock='+data['stock/result']+'；upgrade='+data['upgrade/result']+'（历史回执，不能自动归属本次操作）',
                camera_log='\n\n'.join(summary), raw='\n\n'.join(raw))

def format_context(context, first_failure=''):
    pairs = [('操作','action'),('流程','route'),('阶段','stage'),('原始错误 / 结果','reason'),('电脑退出码','host_exit'),
             ('相机退出码','camera_exit'),('USB','transport'),('目标','target'),('命令','command'),('期望','expected'),('实际','actual'),
             ('运行状态','state'),('系统分区','readonly'),('恢复状态','recovery'),('时间','time')]
    text = ('先前操作首次失败（保留）\n'+first_failure+'\n\n') if first_failure else ''
    text += '\n'.join(label+'：'+str(context.get(key,'未取得')) for label,key in pairs)
    return text+'\n\n相机安装日志：\n'+str(context.get('camera_log','未读取'))+'\n\nUSB 中断或缺少日志时，只表示证据未取得；不会自动重连、重复安装或推定恢复成功。'
