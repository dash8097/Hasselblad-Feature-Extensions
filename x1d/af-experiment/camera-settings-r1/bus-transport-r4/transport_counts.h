/* 仅匿名累计计数；所有字段为 lock-free ARM32 原子，不保留报文或身份。 */
#ifndef AF_TRANSPORT_COUNTS_H
#define AF_TRANSPORT_COUNTS_H
#include "settings_wire.h"
struct TransportCounts {
    enum Field { Calls, Private, Returned, Ok, NotReady, Invalid, Other,
        Queue, SerialTx, SerialRx, Receive, RxOwner, Rx784, Rx784Size,
        Rx784Owner, Rx784Queued, RxArgument, RxWide, UartParse, Count };
    u32 values[Count]={0};u32 dirty=0;
    void add(Field key){
        __atomic_fetch_add(values+key,1u,__ATOMIC_RELAXED);
        __atomic_store_n(&dirty,1u,__ATOMIC_RELEASE);
    }
    u32 get(Field key) const{return __atomic_load_n(values+key,__ATOMIC_RELAXED);}
    bool changed(){return __atomic_exchange_n(&dirty,0u,__ATOMIC_ACQ_REL)!=0;}
    bool begin(const void *data,unsigned length){
        add(Calls);
        if(!data || length!=261)return false;
        const auto *p=static_cast<const unsigned char *>(data);
        if(p[0]!=0x0f || p[1]!=3 || p[2]!=5 || p[3]!=1 || p[4]!=0 || p[5]!=255 ||
           !as_owned(p+6) || as_get(p+14)!=3 ||
           (as_get(p+18)!=AS_QUERY && as_get(p+18)!=AS_APPLY) ||
           as_get(p+257)!=as_hash(p+6,251))return false;
        add(Private);return true;
    }
    void end(bool privateRequest,int result){
        if(!privateRequest)return;
        add(Returned);add(result==0?Ok:(result==-1?NotReady:(result==-2?Invalid:Other)));
    }
    void received(const unsigned char *p,int length,bool owner){
        add(Receive);if(owner)add(RxOwner);
        if(!p){add(RxArgument);return;}
        if(length<2 || length>320)add(RxWide);
        if(length>=2 && p[0]==0x10 && p[1]==3){
            add(Rx784);if(length==259)add(Rx784Size);if(owner)add(Rx784Owner);
        }
    }
};
#endif
