#include "ui_flow.h"
#include <cassert>
#include <cstring>
static NaConfig valid(){NaConfig c={};c.magic=NA_CONFIG_MAGIC;c.abi=NA_CONFIG_ABI;c.lens=75;c.revision=1;c.flags=NA_FAR_FIRST;c.fast_advance_ms=65534;c.fine_advance_ms=65534;c.checksum=na_config_checksum(&c);return c;}
static void checksum(unsigned char *p){as_put(p+251,as_hash(p,251));}
int main(){
    UiFlow f;f.shown=true;assert(f.automatic(0));f.begin(1,0);
    assert(f.reading() && !f.applying() && !f.automatic(1500));
    assert(!f.timeout(2200));assert(f.timeout(2201));assert(!f.applying() && !f.reading() && !f.automatic(9000));
    f.paused=false;f.begin(1,10000);f.queued=true;assert(f.applying());
    assert(f.timeout(12201));assert(!f.queued && !f.automatic(19000));
    f.paused=false;f.begin(1,20000);f.queued=true;f.completed();assert(f.applying());
    f.queued=false;f.begin(2,20100);assert(f.applying() && !f.reading());f.completed();assert(!f.applying());
    f.shown=false;assert(!f.automatic(30000));
    unsigned char p[255]={};NaConfig c=valid(),a={},b={};u32 status=999;
    as_put(p,0x414c4248);as_put(p+4,0x21335246);as_put(p+8,3);as_put(p+12,0x80000001);
    as_put(p+16,51);as_put(p+20,71);as_put(p+36,18);as_config_write(p+44,&c);as_config_write(p+88,&c);checksum(p);
    f.begin(1,0);assert(uiReply(p,f,51,71,1,c,a,b,status)==UI_OK);
    assert(uiReply(p,f,52,71,1,c,a,b,status)==UI_ENVELOPE);
    assert(uiReply(p,f,51,72,1,c,a,b,status)==UI_ENVELOPE);
    assert(uiReply(p,f,51,71,2201,c,a,b,status)==UI_ENVELOPE);
    p[252]^=1;assert(uiReply(p,f,51,71,1,c,a,b,status)==UI_ENVELOPE);checksum(p);
    as_put(p+36,17);checksum(p);assert(uiReply(p,f,51,71,1,c,a,b,status)==UI_LENS);as_put(p+36,18);
    as_put(p+44,0);checksum(p);assert(uiReply(p,f,51,71,1,c,a,b,status)==UI_CONFIG);as_config_write(p+44,&c);
    as_put(p+12,0x80000002);checksum(p);f.begin(2,0);assert(uiReply(p,f,51,71,1,c,a,b,status)==UI_OK);
    NaConfig different=c;different.revision++;different.checksum=na_config_checksum(&different);
    assert(uiReply(p,f,51,71,1,different,a,b,status)==UI_APPLY_MISMATCH);
    as_put(p+24,101);checksum(p);assert(uiReply(p,f,51,71,1,different,a,b,status)==UI_OK && status==101);
}
