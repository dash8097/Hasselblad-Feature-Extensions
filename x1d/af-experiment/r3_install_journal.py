"""R3持久化日志。静态入口加只追加事件链；不替换正在被Windows读取的文件。"""
import sys,os,json,copy,hashlib
from pathlib import Path
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent

def canonical(value):return json.dumps(value,ensure_ascii=False,sort_keys=True,separators=(',',':'))

class InstallJournal:
    def __init__(self,path,artifact_sha256,backend='offline_memory_model'):
        self.path=Path(path).resolve()
        if Path.cwd().resolve()!=HERE.parents[1]:raise ValueError('current workspace required')
        if not self.path.is_relative_to(HERE) or self.path.suffix!='.json':raise ValueError('fixed AF directory required')
        if len(artifact_sha256)!=64 or any(c not in '0123456789abcdef' for c in artifact_sha256):
            raise ValueError('artifact digest required')
        if backend not in ('offline_memory_model','offline_packet_model','fixed_usb'):raise ValueError('fixed backend identity')
        self.events_path=self.path.with_suffix('.events.jsonl');self.lock=self.path.with_suffix('.lock')
        if any(p.exists() for p in (self.path,self.events_path,self.path.with_suffix('.pending'),self.lock)):
            raise RuntimeError('existing journal cannot be reused or resumed')
        self.lock_handle=None;self.events_handle=None;self.closed=False;self.failed=False
        self._event_index=0;self._last_hash='0'*64;self._persisted={}
        self.record={'schema':2,'artifactSha256':artifact_sha256,'phase':'new',
            'sequence':0,'inFlight':None,'lastCompleted':None,
            'automaticResume':False,'automaticRestore':False,'hardwareRequests':0,
            'backend':backend}
        try:
            self.lock_handle=self.lock.open('x',encoding='utf-8')
            self.lock_handle.write('exclusive writer; journal name is never reused\n')
            self.lock_handle.flush();os.fsync(self.lock_handle.fileno())
            # 两个文件均仅创建一次。持有同一写句柄，不执行rename/replace/unlink。
            self.events_handle=self.events_path.open('x',encoding='utf-8',newline='\n')
            anchor={'schema':2,'artifactSha256':artifact_sha256,'backend':backend,
                'eventsFile':self.events_path.name,'format':'append-only-sha256-chain',
                'automaticResume':False,'automaticRestore':False}
            with self.path.open('x',encoding='utf-8',newline='\n') as handle:
                handle.write(canonical(anchor)+'\n');handle.flush();os.fsync(handle.fileno())
            self.persist(self.record)
        except BaseException:self.failed=True;self.close();raise
    def persist(self,record):
        if self.closed or self.failed:raise RuntimeError('journal is closed or failed')
        changes={k:v for k,v in record.items() if k not in self._persisted or self._persisted[k]!=v}
        removed=[k for k in self._persisted if k not in record]
        payload={'index':self._event_index+1,'previous':self._last_hash,'change':changes,'remove':removed}
        digest=hashlib.sha256(canonical(payload).encode('utf-8')).hexdigest()
        line=canonical({**payload,'sha256':digest})+'\n'
        try:
            if self.events_handle.write(line)!=len(line):raise OSError('short journal append')
            self.events_handle.flush();os.fsync(self.events_handle.fileno())
        except BaseException:self.failed=True;raise
        self._event_index+=1;self._last_hash=digest;self._persisted=copy.deepcopy(record)
    def begin(self,phase,operation):
        if self.closed or self.failed or self.record['inFlight'] is not None:
            raise RuntimeError('journal requires inspection; no automatic retry')
        kind,address,value=operation
        if kind not in ('write','cache_model','read','wake_read','version','hold_check') or type(address)!=int or type(value)!=int:
            raise ValueError('fixed transaction operation required')
        record=copy.deepcopy(self.record);record['phase']=phase
        record['sequence']+=1
        record['inFlight']={'kind':kind,'address':address,'value':value,'result':'unknown_until_completed'}
        try:self.persist(record)
        except BaseException:self.failed=True;raise
        self.record=record
    def complete(self):
        if self.closed or self.failed or self.record['inFlight'] is None:raise RuntimeError('pending operation required')
        record=copy.deepcopy(self.record);record['lastCompleted']=record['inFlight'];record['inFlight']=None
        record['lastCompleted']['result']=('offline_model_effect_verified' if record['backend']=='offline_memory_model'
            else 'request_reply_matched_handles_closed')
        try:self.persist(record)
        except BaseException:self.failed=True;raise
        self.record=record
    def close(self):
        if not self.closed:
            if self.events_handle:self.events_handle.close()
            if self.lock_handle:self.lock_handle.close()
            self.closed=True
            # 保留独占标志，该文件名不支持再次取得写入权。
    @staticmethod
    def read_record(path,artifact_sha256):
        path=Path(path).resolve();anchor=json.loads(path.read_text(encoding='utf-8'))
        if anchor.get('artifactSha256')!=artifact_sha256:raise ValueError('journal identity mismatch')
        if anchor.get('schema')==1:
            return {**anchor,'journalAudit':{'format':1,'pendingSnapshotExists':path.with_suffix('.pending').exists()}}
        if anchor.get('schema')!=2 or anchor.get('format')!='append-only-sha256-chain':raise ValueError('journal format')
        events=(path.parent/anchor['eventsFile']).resolve()
        if events.parent!=path.parent or events!=path.with_suffix('.events.jsonl'):raise ValueError('journal event path')
        previous='0'*64;count=0;record={};incomplete=False
        with events.open('r',encoding='utf-8') as handle:
            for line in handle:
                if not line.endswith('\n'):incomplete=True;break
                event=json.loads(line);digest=event.pop('sha256')
                if event['index']!=count+1 or event['previous']!=previous or hashlib.sha256(canonical(event).encode('utf-8')).hexdigest()!=digest:
                    raise ValueError('journal event chain damaged; preserve files')
                for key in event['remove']:record.pop(key,None)
                record.update(event['change']);count+=1;previous=digest
        if not count:raise ValueError('journal has no durable initial record')
        if record.get('artifactSha256')!=artifact_sha256:raise ValueError('journal record identity')
        return {**record,'journalAudit':{'format':2,'verifiedEvents':count,'lastSha256':previous,
            'incompleteTail':incomplete,'eventsFile':events.name}}
    @staticmethod
    def inspect(path,artifact_sha256):
        record=InstallJournal.read_record(path,artifact_sha256)
        return {'sequence':record['sequence'],'phase':record['phase'],
            'inFlight':record['inFlight'],'cacheInFlight':record.get('cacheInFlight'),
            'automaticResume':False,'automaticRestore':False,'journalAudit':record['journalAudit'],
            'nextAction':'offline_review_required','hardwareRequests':record.get('hardwareRequests',0)}
