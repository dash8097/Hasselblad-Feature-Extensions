"""仅对冻结副本应用相册流式核验；不修改相机或原始源码。"""


def replace_once(text, old, new):
    assert text.count(old) == 1, "album stream patch baseline changed"
    return text.replace(old, new)


def patch(name, text):
    if name == "native_common.hpp":
        return replace_once(
            text, "Process command(const std::vector<std::string>&,unsigned=6000,bool=true);",
            "Process command(const std::vector<std::string>&,unsigned=6000,bool=true);\n"
            "Process album_contains(const std::string&,const std::string&);")
    if name == "native_common.cpp":
        text = replace_once(text,
            "Process command(const std::vector<std::string>&args,unsigned timeout,bool honour_cancel){",
            "static Process command_impl(const std::vector<std::string>&args,unsigned timeout,bool honour_cancel,const std::string&needle){")
        text = replace_once(text,
            "Process result={1,{}};bool done=false,eof=false,killed=false;",
            "Process result={1,{}};std::string tail;size_t received=0;bool matched=false;\n"
            "    bool done=false,eof=false,killed=false;")
        text = replace_once(text,
            'if(n>0){require(result.output.size()+size_t(n)<=1024*1024,"command-output-limit");result.output.append(b,size_t(n));}',
            '''if(n>0){
                if(needle.empty()){
                    require(result.output.size()+size_t(n)<=1024*1024,"command-output-limit");
                    result.output.append(b,size_t(n));
                }else{
                    require(received+size_t(n)<=128u*1024u*1024u,"album-output-limit");
                    received+=size_t(n);
                    tail.append(b,size_t(n));
                    matched=matched||tail.find(needle)!=std::string::npos;
                    if(tail.size()>=needle.size())tail.erase(0,tail.size()-(needle.size()-1));
                }
            }''')
        text = replace_once(text,
            '    return result;\n}\nstd::string get(',
            '''    if(!needle.empty())result.output="matched="+std::string(matched?"1":"0")+"\\nbytes="+std::to_string(received)+"\\n";
    return result;
}
Process command(const std::vector<std::string>&args,unsigned timeout,bool honour_cancel){
    return command_impl(args,timeout,honour_cancel,{});
}
Process album_contains(const std::string&folder,const std::string&image){
    require(!folder.empty()&&folder.size()<128&&folder.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")==std::string::npos,"album-folder",2);
    require(!image.empty()&&image.size()<128&&image.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-")==std::string::npos,"album-image",2);
    return command_impl({"/bin/dbus-send","--system","--print-reply","--reply-timeout=5000","--dest=com.hasselblad.storage","/storage","com.hasselblad.storage.browse","string:/ssd/"+folder,"int32:0"},6000,true,"string \\""+image+"\\"");
}
std::string get(''')
        return text
    if name == "native_publish.cpp":
        text = replace_once(text,
            'auto r=command({"/bin/dbus-send","--system","--print-reply","--reply-timeout=5000","--dest=com.hasselblad.storage","/storage","com.hasselblad.storage.browse","string:/ssd/"+p.folder,"int32:0"});',
            'auto r=album_contains(p.folder,p.image);')
        return replace_once(text,
            'return r.output.find("string \\""+p.image+"\\"")!=std::string::npos;',
            'return r.output.rfind("matched=1\\n",0)==0;')
    return text
