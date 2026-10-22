#include "native_publish.cpp"
#include <sys/reboot.h>
static unsigned mode,queries;static uint64_t ticks;
namespace cn {
void require(bool b,const char*s,int r){if(!b)throw Error(s,r);}
void require(bool b,const std::string&s,int r){if(!b)throw Error(s,r);}
uint64_t monotonic_ms(){return ticks;}
void sleep_ms(unsigned n){ticks+=n;}
std::string get(const std::string&,const std::string&p,const std::string&,unsigned){
 if(p=="storage_status"){++queries;
  if(mode==1&&queries<5||mode==2)throw Error("dbus-get:storage.storage_status");
  if(mode==3)throw Error("cancelled",143);
  if(mode==4)throw Error("dbus-variant:storage_status");
 }
 if(p=="active_error")return mode==5?"true":"false";
 return p=="stream_group"?"255":"0";
}
}
int main(){unsigned checks=0;for(mode=0;mode<6;mode++){ticks=queries=0;bool ok=false;int rc=0;std::string detail;
 try{cn::wait_storage_idle();ok=true;}catch(const cn::Error&e){rc=e.rc;detail=e.what();}
 bool pass=mode<2?ok:(mode==2||mode==5)?!ok&&detail=="refresh-ready-timeout"&&ticks==20000:mode==3?!ok&&rc==143&&ticks==0:!ok&&detail=="dbus-variant:storage_status"&&ticks==0;
 if(mode==1)pass=pass&&queries==5&&ticks==1000;
 if(!pass){printf("STARTUP_FAILED mode=%u\n",mode);fflush(stdout);reboot(RB_POWER_OFF);return 1;}++checks;
 }printf("STARTUP_PASSED checks=%u\n",checks);fflush(stdout);reboot(RB_POWER_OFF);return 0;}
