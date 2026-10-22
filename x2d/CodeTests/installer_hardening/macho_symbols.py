"""Erase only non-exported Mach-O symbol names; retain indices, bind tables and layout.

Zig 0.13's -s leaves local names in these dylibs. No instruction, relocation,
export or undefined symbol is changed. Only existing ad-hoc SHA-256 signatures
are refreshed; signed/nonstandard files fail closed. This is not Developer ID signing.
"""
import hashlib
import struct

def sanitize(data):
    out=bytearray(data)
    if out[:4]!=b'\xcf\xfa\xed\xfe':raise ValueError('Expected little-endian 64-bit Mach-O')
    ncmds=struct.unpack_from('<I',out,16)[0];offset=32;sym=None;signature=None
    for _ in range(ncmds):
        command,size=struct.unpack_from('<II',out,offset)
        if size<8 or offset+size>len(out):raise ValueError('Invalid load command')
        if command==2:sym=struct.unpack_from('<IIII',out,offset+8)
        if command==0x1d:signature=struct.unpack_from('<II',out,offset+8)
        offset+=size
    if sym is None:raise ValueError('Missing symbol table')
    symoff,count,stroff,strsize=sym
    if symoff+count*16>len(out) or stroff+strsize>len(out):raise ValueError('Symbol bounds')
    protected=set();locals_=[]
    for index in range(count):
        start=symoff+index*16
        string_index,kind=struct.unpack_from('<IB',out,start)
        if string_index==0:continue
        if string_index>=strsize:raise ValueError('String bounds')
        end=out.find(0,stroff+string_index,stroff+strsize)
        if end<0:raise ValueError('Unterminated symbol')
        positions=range(stroff+string_index,end)
        if kind&1 or (kind&0x0e)==0:protected.update(positions)
        else:locals_.append((start,positions))
    for start,positions in locals_:
        struct.pack_into('<I',out,start,0)
        for p in positions:
            if p not in protected:out[p]=0
    if signature:
        base,length=signature
        if base+length>len(out):raise ValueError('Signature bounds')
        magic,total,entries=struct.unpack_from('>III',out,base)
        if magic!=0xfade0cc0 or total>length:raise ValueError('Unsupported signature')
        directories=0
        for i in range(entries):
            slot,relative=struct.unpack_from('>II',out,base+12+8*i)
            if slot==0x10000:raise ValueError('Refuse CMS-signed input')
            cd=base+relative
            if struct.unpack_from('>I',out,cd)[0]!=0xfade0c02:continue
            _,cdsize,version,flags,hashoff,ident,special,slots,limit=struct.unpack_from('>9I',out,cd)
            hashsize,hashtype,platform,page=struct.unpack_from('4B',out,cd+36)
            if not flags&2 or hashtype!=2 or hashsize!=32 or limit>base or page>20:
                raise ValueError('Expected bounded ad-hoc SHA-256 signature')
            pagesize=1<<page
            if slots!=(limit+pagesize-1)//pagesize or hashoff+slots*32>cdsize:raise ValueError('CodeDirectory bounds')
            for n in range(slots):
                begin=n*pagesize;digest=hashlib.sha256(out[begin:min(begin+pagesize,limit)]).digest()
                out[cd+hashoff+n*32:cd+hashoff+(n+1)*32]=digest
            directories+=1
        if not directories:raise ValueError('CodeDirectory missing')
    return bytes(out),len(locals_)
