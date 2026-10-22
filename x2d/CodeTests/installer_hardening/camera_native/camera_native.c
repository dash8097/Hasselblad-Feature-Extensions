#include "camera_native.h"
#include <string.h>

int cn_job_valid(const char *s) {
    static const char prefix[]="/mnt/media_rw/ssd/multishot/native-album-ui-";
    if (!s || strncmp(s,prefix,sizeof(prefix)-1)) return 0;
    s+=sizeof(prefix)-1;
    for (unsigned i=0;i<8;i++,s++)
        if (!((*s>='0' && *s<='9') || (*s>='a' && *s<='f'))) return 0;
    if (*s++!='-' || *s<'1' || *s>'9') return 0;
    unsigned count=0;
    do { count++;s++; } while (*s>='0' && *s<='9');
    return !*s && count<=10;
}

/* 不将截断、重复字段或整数溢出视为足够内存。只接受 proc 的 KiB 单位。 */
int cn_mem_available(const char *text,size_t length,uint64_t *out) {
    size_t pos=0;int seen=0;uint64_t result=0;
    if (!text || !out || memchr(text,0,length)) return 0;
    while(pos<length) {
        size_t end=pos;while(end<length && text[end]!='\n') end++;
        if(end-pos>=13 && !memcmp(text+pos,"MemAvailable:",13)) {
            if(seen++) return 0;
            size_t p=pos+13;
            while(p<end && (text[p]==' ' || text[p]=='\t')) p++;
            size_t start=p;uint64_t value=0;
            while(p<end && text[p]>='0' && text[p]<='9') {
                unsigned digit=(unsigned)(text[p++]-'0');
                if(value>(UINT64_MAX-digit)/10) return 0;
                value=value*10+digit;
            }
            if(p==start || p==end || (text[p]!=' ' && text[p]!='\t')) return 0;
            while(p<end && (text[p]==' ' || text[p]=='\t')) p++;
            if(end-p<2 || memcmp(text+p,"kB",2)) return 0;
            p+=2;while(p<end && (text[p]==' ' || text[p]=='\t' || text[p]=='\r')) p++;
            if(p!=end) return 0;
            result=value;
        }
        pos=end+1;
    }
    if(!seen) return 0;
    *out=result;return 1;
}

struct cn_result cn_run(int argc,const char *const *argv,const struct cn_ops *o) {
    struct cn_result r={2,CN_ARGS,0};
    if(!o || !argv) return r;
    if(argc==2 && !strcmp(argv[1],"audio")) {
        if(o->cancelled(o->ctx)) {r.rc=143;return r;}
        r.phase=CN_ENCODE;r.rc=o->audio(o->ctx);return r;
    }
    if(argc!=3 || strcmp(argv[1],"heif-disk") || !cn_job_valid(argv[2])) return r;
    if(o->cancelled(o->ctx)) {r.rc=143;return r;}
    r.phase=CN_VALIDATE;r.rc=o->prepare(o->ctx,argv[2]);
    if(r.rc) return r;
    r.phase=CN_MEMORY;
    for(unsigned n=0;n<50;n++) {
        uint64_t available=0;
        if(o->cancelled(o->ctx)) {r.rc=143;return r;}
        r.memory_reads++;
        if(!o->memory(o->ctx,&available)) {r.rc=19;return r;}
        if(available>=573440) break;
        if(n==49) {
            o->event(o->ctx,"NATIVE_DISK_HEIF_MEMORY_UNAVAILABLE_JPEG_RETAINED");
            r.rc=19;return r;
        }
        if(!o->delay(o->ctx)) {r.rc=o->cancelled(o->ctx)?143:1;return r;}
    }
    if(o->cancelled(o->ctx)) {r.rc=143;return r;}
    r.phase=CN_ENCODE;r.rc=o->encode(o->ctx,argv[2]);
    if(o->cancelled(o->ctx)) {r.rc=143;return r;}
    if(r.rc) return r;
    r.phase=CN_VERIFY;r.rc=o->verify(o->ctx);
    if(o->cancelled(o->ctx)) {r.rc=143;return r;}
    if(r.rc) return r;
    o->event(o->ctx,"HEIF_ENCODED_DIMENSIONS_VERIFIED");
    r.phase=CN_DONE;return r;
}
