static std::vector<unsigned char> query(unsigned seq=456){
    std::vector<unsigned char> p(255,0);
    as_put(p.data(),0x414c4248);as_put(p.data()+4,0x21335346);as_put(p.data()+8,3);
    as_put(p.data()+12,1);as_put(p.data()+16,123);as_put(p.data()+20,seq);
    as_put(p.data()+251,as_hash(p.data(),251));return p;
}
static void receive(Bus &b,std::vector<unsigned char> p){incoming.push_back({1,p});b.receive();}
static QByteArray reply(unsigned seq=456){
    QByteArray p(259,0);p[0]=0x10;p[1]=3;p[2]=1;p[3]=5;
    auto *r=reinterpret_cast<unsigned char *>(p.data()+4);
    as_put(r,0x414c4248);as_put(r+4,0x21335246);as_put(r+8,3);as_put(r+12,0x80000001);
    as_put(r+16,123);as_put(r+20,seq);as_put(r+251,as_hash(r,251));return p;
}
int main(){
    QObject owner;
    {Bus b(&owner);assert(!b.pending && invocations==0);receive(b,query());
        assert(b.pending && b.queries==1 && b.uartAccepted==1 && invocations==1);
        assert(sent.size()==261 && sent[0]==15 && sent[1]==3 && sent[2]==5 && sent[3]==1 && sent[4]==0 && sent[5]==255);
        auto q=query();assert(!std::memcmp(sent.data()+6,q.data(),255));
        receive(b,query(457));assert(b.busyRejected==1 && invocations==1);
        b.reply(reply(457));assert(b.replyRejected==1 && b.pending);
        auto bad=reply();bad[258]^=1;b.reply(bad);assert(b.replyRejected==2 && b.pending);
        b.reply(reply());assert(!b.pending && b.matched==1 && b.uiDelivered==1 && returned.size()==255);
        b.reply(reply());assert(b.unsolicited==1 && b.uiDelivered==1);
    }
    {Bus b(&owner);accepted=false;receive(b,query());
        assert(!b.pending && b.uartRejected==1 && b.uartAccepted==0);accepted=true;
        receive(b,query(457));assert(b.pending && b.uartAccepted==1);}
    {Bus b(&owner);invoked=false;receive(b,query());assert(!b.pending && b.invokeFailed==1);invoked=true;}
    {Bus b(&owner);unsigned n=invocations;
        incoming.push_back({0,{}});b.receive();assert(b.socketRejected==1 && invocations==n);
        auto q=query();q[252]^=1;receive(b,q);assert(b.envelopeRejected==1 && invocations==n);
        q=query();q[80]=1;as_put(q.data()+251,as_hash(q.data(),251));receive(b,q);
        assert(b.configRejected==1 && invocations==n);}
    {Bus b(&owner);receive(b,query());now+=2001;b.reply(reply());assert(b.expired==1 && b.uiDelivered==0 && !b.pending);}
    {Bus b(&owner);receive(b,query());delivered=false;b.reply(reply());assert(b.uiFailed==1 && b.matched==1 && !b.pending);delivered=true;}
    {Bus b(&owner);for(unsigned i=0;i<8;i++)incoming.push_back({0,{}});b.receive();
        assert(b.datagrams==4 && incoming.size()==4);b.receive();assert(b.datagrams==8 && incoming.empty());}
    {TransportCounts c;std::vector<unsigned char> p(261,0);auto q=query();
        p[0]=15;p[1]=3;p[2]=5;p[3]=1;p[5]=255;std::memcpy(p.data()+6,q.data(),255);
        assert(!c.begin(nullptr,0) && !c.begin(p.data(),260));
        for(int result:{0,-1,-2,19}){assert(c.begin(p.data(),261));c.end(true,result);}
        assert(c.get(TransportCounts::Calls)==6 && c.get(TransportCounts::Private)==4);
        assert(c.get(TransportCounts::Returned)==4 && c.get(TransportCounts::Ok)==1);
        assert(c.get(TransportCounts::NotReady)==1 && c.get(TransportCounts::Invalid)==1 && c.get(TransportCounts::Other)==1);
        p[260]^=1;assert(!c.begin(p.data(),261));c.end(false,-1);assert(c.get(TransportCounts::Returned)==4);
        assert(c.changed() && !c.changed());}
    {TransportCounts c;unsigned char id[]={0x10,3};
        c.received(nullptr,0,true);c.received(id,1,true);c.received(id,259,true);c.received(id,321,false);
        assert(c.get(TransportCounts::Receive)==4 && c.get(TransportCounts::RxOwner)==3);
        assert(c.get(TransportCounts::Rx784)==2 && c.get(TransportCounts::Rx784Size)==1 && c.get(TransportCounts::Rx784Owner)==1);
        assert(c.get(TransportCounts::RxArgument)==1 && c.get(TransportCounts::RxWide)==2);}
    puts("actual Bus methods: 7 groups; transport classification: 2 groups passed");
}
