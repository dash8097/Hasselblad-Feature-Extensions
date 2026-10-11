#ifndef HBL_LINK_EVIDENCE_H
#define HBL_LINK_EVIDENCE_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace coexist {
// wpa_supplicant 控制事件仅提取种类；不输出网络名称、地址或原文。
enum class LinkEvent { Other, Connected, Disconnected, Associating, Terminating, Overflow };
inline LinkEvent parseEvent(const char *text,size_t size) {
    if(!text || !size || size>4096 || memchr(text,0,size)) return LinkEvent::Other;
    size_t i=0;
    if(size>=3 && text[0]=='<' && text[1]>='0' && text[1]<='9' && text[2]=='>') i=3;
    struct Entry {const char *prefix;LinkEvent event;};
    const Entry table[]={{"CTRL-EVENT-CONNECTED",LinkEvent::Connected},
        {"CTRL-EVENT-DISCONNECTED",LinkEvent::Disconnected},
        {"CTRL-EVENT-ASSOC-REJECT",LinkEvent::Associating},
        {"CTRL-EVENT-TERMINATING",LinkEvent::Terminating}};
    for(const auto &entry:table) {
        size_t n=strlen(entry.prefix);
        if(size-i>=n && !memcmp(text+i,entry.prefix,n) &&
           (size-i==n || text[i+n]==' ' || text[i+n]=='\n')) return entry.event;
    }
    return LinkEvent::Other;
}
struct Evidence {
    uint32_t connects=0,disconnects=0,associationFailures=0,observed=0;
    bool gap=false,attached=false,baselineReady=false,recovered=false;
    uint64_t awayAt=0,returnAt=0;
    void event(LinkEvent e) {
        ++observed;
        if(e==LinkEvent::Connected) ++connects;
        if(e==LinkEvent::Disconnected) ++disconnects;
        if(e==LinkEvent::Associating) ++associationFailures;
        if(e==LinkEvent::Terminating || e==LinkEvent::Overflow) {gap=true;attached=false;}
    }
    // 只能说“观察窗口内未收到断线事件”，不能由两端快照推定持续连接。
    bool noObservedDisconnect() const {return attached && baselineReady && recovered && !gap && !disconnects && !connects;}
};
}
#endif
