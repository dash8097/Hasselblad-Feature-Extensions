#define HBL_DIRECT_DISPATCH_TEST_PLATFORM "mechanical_direct_fake_platform.h"
#include "mechanical_direct_dispatch.h"

namespace test=hbl_direct_test;
using Dispatch=MechanicalDirectDispatch;
static unsigned checks=0;
static void check(bool correct,const char *message) {
    if (!correct) throw std::runtime_error(message);
    ++checks;
}
struct Socket {
    int fd=test::add(test::State::Socket);
    std::shared_ptr<test::State> state;
    Socket() { std::lock_guard<std::mutex> lock(test::os().mutex); state=test::file(fd); }
    ~Socket() { if(fd>=0) test::closeFile(fd); }
    unsigned attempts() { std::lock_guard<std::mutex> lock(test::os().mutex); return state->attempts; }
};
static sockaddr address={1,{}};
static const uint8_t packet[]={3,7,11,19};
static bool schedule(Dispatch &dispatch,Socket &socket,uint64_t target) {
    return dispatch.schedule(socket.fd,packet,sizeof(packet),&address,sizeof(address),target);
}
static Dispatch::Result completed(Dispatch &dispatch) {
    test::PollFd ready={dispatch.completionFd(),POLLIN,0};
    check(test::pollFiles(&ready,1,2000)==1,"completion notification missing");
    return dispatch.take();
}
int main() {
    try {
        {
            Dispatch direct; Socket socket;
            check(direct.valid(),"dedicated thread unavailable");
            check(!schedule(direct,socket,0),"zero deadline accepted");
            check(!schedule(direct,socket,test::now()+6000000),"unbounded wait accepted");
            check(!schedule(direct,socket,test::now()-300000),"stale deadline accepted");
            check(!direct.schedule(socket.fd,packet,257,&address,sizeof(address),test::now()+10000),"oversized packet accepted");
            const auto at=test::now()+50000;
            check(schedule(direct,socket,at),"valid deadline refused");
            check(!schedule(direct,socket,at+10000),"pending request replaced");
            check(completed(direct)==Dispatch::Submitted,"valid request did not submit");
            check(test::now()>=at && socket.attempts()==1,"early or repeated submit");
            check(direct.take()==Dispatch::Empty && direct.cancel()==Dispatch::Empty,"result consumed twice");
            check(socket.state->bytes==std::vector<uint8_t>(packet,packet+sizeof(packet)),"preencoded bytes changed");
        }
        {
            Dispatch direct; Socket socket;
            check(schedule(direct,socket,test::now()+100000),"cancellation setup refused");
            check(direct.cancel()==Dispatch::Cancelled,"unsubmitted cancellation failed");
            std::this_thread::sleep_for(std::chrono::milliseconds(120));
            check(socket.attempts()==0 && direct.take()==Dispatch::Empty,"cancelled request submitted");
            const auto at=test::now()+20000;
            check(schedule(direct,socket,at),"replacement refused");
            check(completed(direct)==Dispatch::Submitted && test::now()>=at,"old timer fired replacement early");
        }
        {
            Dispatch direct; Socket socket;
            check(schedule(direct,socket,test::now()+30000),"fd ownership setup failed");
            test::closeFile(socket.fd); socket.fd=-1;
            check(completed(direct)==Dispatch::Submitted && socket.attempts()==1,"caller close invalidated owned fd");
        }
        {
            Dispatch direct; Socket socket;
            { std::lock_guard<std::mutex> lock(test::os().mutex); test::os().failSend=true; }
            check(schedule(direct,socket,test::now()+10000),"failure setup failed");
            check(completed(direct)==Dispatch::Failed,"ambiguous send failure reported success");
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            check(socket.attempts()==1,"send failure retried");
        }
        {
            Dispatch direct; Socket socket;
            std::mutex gate; std::condition_variable changed;
            bool entered=false,release=false,cancelReturned=false;
            {
                std::lock_guard<std::mutex> lock(test::os().mutex);
                test::os().sendBarrier=[&]() {
                    std::unique_lock<std::mutex> lock(gate);
                    entered=true; changed.notify_all();
                    changed.wait(lock,[&]() { return release; });
                };
            }
            check(schedule(direct,socket,test::now()+10000),"race setup failed");
            { std::unique_lock<std::mutex> lock(gate); check(changed.wait_for(lock,std::chrono::seconds(2),[&]() { return entered; }),"send did not enter"); }
            Dispatch::Result cancelled=Dispatch::Empty;
            std::thread concurrent([&]() {
                cancelled=direct.cancel();
                std::lock_guard<std::mutex> lock(gate); cancelReturned=true;
            });
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            bool blocked;
            { std::lock_guard<std::mutex> lock(gate); blocked=!cancelReturned; release=true; changed.notify_all(); }
            concurrent.join();
            check(blocked,"cancel returned while submit ownership ambiguous");
            check(cancelled==Dispatch::Submitted && socket.attempts()==1,"committed request falsely cancelled");
            { std::lock_guard<std::mutex> lock(test::os().mutex); test::os().sendBarrier=nullptr; }
        }
        {
            Socket socket;
            { Dispatch direct; check(schedule(direct,socket,test::now()+100000),"shutdown setup failed"); }
            std::this_thread::sleep_for(std::chrono::milliseconds(120));
            check(socket.attempts()==0,"destructor allowed later submission");
        }
        {
            { std::lock_guard<std::mutex> lock(test::os().mutex); test::os().rejectThread=true; }
            { Dispatch direct; Socket socket; check(!direct.valid() && !schedule(direct,socket,test::now()+10000),"realtime scheduling refusal bypassed"); }
            { std::lock_guard<std::mutex> lock(test::os().mutex); test::os().rejectThread=false; }
        }
        {
            std::lock_guard<std::mutex> lock(test::os().mutex);
            check(test::os().files.empty(),"file descriptor leak");
            check(test::os().createdThreads==test::os().joinedThreads,"thread leak");
        }
        std::cout<<"direct-dispatch-checks="<<checks<<" device-requests=0\n";
        return 0;
    } catch (const std::exception &failure) {
        std::cerr<<failure.what()<<"\n"; return 1;
    }
}
