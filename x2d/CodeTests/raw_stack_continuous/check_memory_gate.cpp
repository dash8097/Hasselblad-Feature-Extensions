#include "native_memory_request.hpp"
#include "memory_request_gate.h"
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <cstdio>
extern "C" int cn_cancel_requested(){return 0;}
extern "C" int cn_signal_pending(){return 0;}
extern "C" void cn_recovery_mode(int){}
static unsigned checks;
static void ck(bool v){++checks;if(!v){printf("GATE_FAIL %u\n",checks);fflush(stdout);reboot(RB_POWER_OFF);_exit(1);}}
int main(){setvbuf(stdout,nullptr,_IONBF,0);mkdir("/tmp",0700);mount("tmpfs","/tmp","tmpfs",0,nullptr);
 const std::string s="/tmp/job",p=s+"/capture.requested.pending",g=s+"/capture.requested";mkdir(s.c_str(),0700);
 ck(x2d_memory_request_gate(s.c_str())==0);
 cn::write_new(p,"");ck(x2d_memory_request_gate(s.c_str())==0);
 cn::write_replace(p,"6");ck(x2d_memory_request_gate(s.c_str())==0);
 cn::write_replace(p,"6\n");ck(x2d_memory_request_gate(s.c_str())==0);
 cn::write_new(g,"");ck(x2d_memory_request_gate(s.c_str())==1);
 cn::write_replace(p,"6");ck(x2d_memory_request_gate(s.c_str())==-1);
 cn::write_replace(p,"6\nX");ck(x2d_memory_request_gate(s.c_str())==-1);
 cn::write_replace(p,"4\n");ck(x2d_memory_request_gate(s.c_str())==-1);
 cn::write_replace(p,"6\n");cn::write_replace(g,"x");ck(x2d_memory_request_gate(s.c_str())==-1);
 unlink(g.c_str());unlink(p.c_str());cn::memory_request_publish(s);ck(x2d_memory_request_gate(s.c_str())==1);
 bool refused=false;try{cn::memory_request_publish(s);}catch(const cn::Error&){refused=true;}ck(refused);ck(cn::read(p)=="6\n");
 unlink(g.c_str());unlink(p.c_str());cn::write_new(p,"FOREIGN");refused=false;try{cn::memory_request_publish(s);}catch(const cn::Error&){refused=true;}ck(refused);ck(cn::read(p)=="FOREIGN");ck(!cn::exists(g));
 unlink(p.c_str());cn::write_new(g,"FOREIGN");refused=false;try{cn::memory_request_publish(s);}catch(const cn::Error&){refused=true;}ck(refused);ck(cn::read(g)=="FOREIGN");ck(!cn::exists(p));
 unlink(g.c_str());symlink("/tmp/absent",g.c_str());refused=false;try{cn::memory_request_publish(s);}catch(const cn::Error&){refused=true;}ck(refused);ck(!cn::exists(p));
 printf("MEMORY_GATE_PASSED checks=%u\n",checks);sync();reboot(RB_POWER_OFF);return 0;
}
