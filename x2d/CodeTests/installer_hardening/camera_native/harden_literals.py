"""只转换白名单诊断调用的完整普通字符串参数；不改路径、协议或全局初始化。"""
import re
TOKEN=re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|R"[^\n]*?"|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[A-Za-z_][A-Za-z_0-9]*|[^\s]',re.MULTILINE)
ARG={'require':1,'Error':0,'fprintf':1,'printf':0,'puts':0,'ok':1,'syscall_ok':1}

def transform(source):
    tokens=[m for m in TOKEN.finditer(source) if not m.group().startswith(('//','/*'))]
    selected={}
    for at,t in enumerate(tokens[:-1]):
        name=t.group()
        if name not in ARG or tokens[at+1].group()!='(':continue
        start=at+2;depth=[];args=[]
        for end in range(start,len(tokens)):
            token=tokens[end].group()
            if token==')' and not depth:
                args.append((start,end));break
            if token==',' and not depth:args.append((start,end));start=end+1
            elif token in ('(','[','{'):depth.append(token)
            elif token in (')',']','}'):
                if not depth:break
                depth.pop()
        pos=ARG[name]
        if pos>=len(args):continue
        # 保留编译器的 -Wformat-security 门禁；无可变参数的格式字符串不转换。
        if name in ('printf','fprintf') and len(args)==pos+1:continue
        begin,end=args[pos]
        if begin==end:continue
        literals=tokens[begin:end]
        if not all(x.group().startswith('"') for x in literals):continue
        a,b=literals[0].start(),literals[-1].end()
        # 拼接仅包含普通字符串，保留原来的 C++ 转义及相邻字面量语义。
        selected[a]=(b,'CN_HIDDEN_LITERAL('+source[a:b]+')')
    for start,(end,replacement) in sorted(selected.items(),reverse=True):source=source[:start]+replacement+source[end:]
    if selected:source='#include "native_literal.hpp"\n'+source
    return source,len(selected)

def self_test():
    text='require(f(a,b),"hello\\n",2); fprintf(stderr,"x=%d",x); Error("a" "b",90); const char*p="remain"; // puts("comment")\n'
    result,n=transform(text)
    assert n==3 and 'CN_HIDDEN_LITERAL("a" "b")' in result
    assert 'const char*p="remain"' in result and '// puts("comment")' in result
    text='require(fn({1,2}),std::string("dynamic")); puts("a"+s); printf(fmt);'
    assert transform(text)==(text,0)
    return True
if __name__=='__main__':assert self_test();print('literal transformation boundary tests passed')
