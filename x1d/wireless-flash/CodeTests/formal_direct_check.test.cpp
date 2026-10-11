#define HBL_DIRECT_DISPATCH_TEST_PLATFORM "formal_direct_fake_platform.h"
#include "../native/formal_direct_check.h"
namespace test=formal_direct_test;
static unsigned checks=0;
static void check(bool okay,const char *why) { ++checks;if(!okay) throw std::runtime_error(why); }
static void reset() {
    check(test::base::os().files.empty(),"descriptor leak");
    check(test::bindings.empty() && test::queues.empty(),"socket state leak");
    test::sends=test::duplicateAt=test::corruptAt=0;
}
static void cancelled(unsigned stage) {
    if(stage==1 || stage==4) test::parkDriver();
    if(stage==3 || stage==6) test::resumeDriver();
}
static void submitted(unsigned stage) {
    if(stage==2 || stage==5) test::waitForNextPacket();
}
int main() {
    try {
        reset();test::point=cancelled;
        check(formal_direct_check()==0,"cancel-wins selfcheck failed");
        check(test::submittedCount()==3,"cancelled round emitted a packet");
        reset();test::point=submitted;
        check(formal_direct_check()==0,"submit-wins cancellation misreported");
        check(test::submittedCount()==5,"submitted cancellation was lost or repeated");
        reset();test::point=[](unsigned stage) { cancelled(stage);if(stage==7) test::waitForNextPacket(); };
        check(formal_direct_check()==0,"late noEarly observation misreported early send");
        check(test::submittedCount()==3,"late observation duplicated packet");
        reset();test::point=cancelled;test::duplicateAt=1;
        check(formal_direct_check()==86,"duplicate packet accepted");
        reset();test::point=cancelled;test::corruptAt=1;
        check(formal_direct_check()==86,"wrong fixed packet accepted");
        reset();test::point=submitted;test::duplicateAt=2;
        check(formal_direct_check()==88,"duplicate submitted-cancel packet accepted");
        reset();test::point=submitted;test::corruptAt=4;
        check(formal_direct_check()==91,"wrong short-cancel packet accepted");
        reset();test::point=[](unsigned stage) {
            cancelled(stage);
            if(stage==7) { test::parkDriver();test::injectEarlyPacket();test::resumeDriver(); }
        };
        check(formal_direct_check()==93,"early packet accepted");
        reset();
        check(test::base::os().createdThreads==test::base::os().joinedThreads,"thread leak");
        std::cout<<"formal-direct-host-checks="<<checks<<" hardware-requests=0\n";return 0;
    } catch(const std::exception &e) { test::resumeDriver();std::cerr<<e.what()<<"\n";return 1; }
}
