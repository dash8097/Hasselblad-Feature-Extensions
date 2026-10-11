#ifndef HBL_AF_UI_FLOW_R4_H
#define HBL_AF_UI_FLOW_R4_H
extern "C" {
#include "settings_wire.h"
}
/* Pure host UI state; no transport, firmware action or retry is performed here. */
struct UiFlow {
    bool shown=false, paused=false, queued=false;
    unsigned operation=0;
    long long sentAt=0,lastQuery=-1000;
    bool reading() const { return operation==1; }
    bool applying() const { return operation==2 || queued; }
    bool pending() const { return operation!=0; }
    bool automatic(long long now) const { return shown && !paused && !pending() && !queued && now-lastQuery>=1000; }
    void begin(unsigned op,long long now) { operation=op;sentAt=now;if(op==1)lastQuery=now; }
    void completed() { operation=0; }
    bool timeout(long long now) {
        if(!pending() || now-sentAt<=2200)return false;
        operation=0;queued=false;paused=true;return true;
    }
    void failed() { operation=0;queued=false;paused=true; }
};
enum { UI_OK=0,UI_ENVELOPE=1,UI_CONFIG=2,UI_LENS=3,UI_APPLY_MISMATCH=4 };
static inline int uiReply(const unsigned char *p,const UiFlow &flow,u32 session,u32 sequence,long long now,
                          const NaConfig &submitted,NaConfig &config,NaConfig &active,u32 &status){
    if(!flow.pending() || !as_reply(p) || as_get(p+8)!=NA_CONFIG_ABI || as_get(p+12)!=(flow.operation|0x80000000u) ||
       as_get(p+16)!=session || as_get(p+20)!=sequence || as_get(p+251)!=as_hash(p,251) || now-flow.sentAt>2200)return UI_ENVELOPE;
    as_config_read(&config,p+AS_PENDING_OFFSET);as_config_read(&active,p+AS_ACTIVE_OFFSET);status=as_get(p+24);
    if(na_config_validate(&config) || na_config_validate(&active))return UI_CONFIG;
    if(as_get(p+36)!=18)return UI_LENS;
    if(flow.operation==2 && !status){
        const u32 *left=reinterpret_cast<const u32 *>(&config),*right=reinterpret_cast<const u32 *>(&submitted);
        for(unsigned i=0;i<NA_CONFIG_WORDS;i++)if(left[i]!=right[i])return UI_APPLY_MISMATCH;
    }
    return UI_OK;
}
#endif
