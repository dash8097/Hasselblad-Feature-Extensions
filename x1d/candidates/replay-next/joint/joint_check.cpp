#include "joint_policy.h"

static bool check(unsigned actual,uint64_t &sample) {
    char b[160],extra;unsigned pid=0;int ready=0,max=0,bgra=0,npot=0;
    unsigned long long timestamp=0;
    if(!privateDirectory(sessionRoot) || !privateDirectory(sessionState) ||
       !privateRead("/tmp/hbl-x1d-combined/replay-state/ui",b,sizeof(b)) ||
       sscanf(b,"RPU1 %u %d %c",&pid,&ready,&extra)!=2 || pid!=actual || ready!=1) return false;
    if(!privateRead(gpuPath,b,sizeof(b)) ||
       sscanf(b,"RPG1 %u %llu %d %d %d %c",&pid,&timestamp,&max,&bgra,&npot,&extra)!=5 || pid!=actual) return false;
    uint64_t now=nowMs();
    sample=timestamp;
    return timestamp && now>=timestamp && now-timestamp<=2000 && bgra==1 && npot==1 && gpuAllowed(max,true,true);
}
int main(int argc,char **argv) {
    if(argc!=3 || strcmp(argv[1],"--gpu")) return 59;
    char *end=nullptr;unsigned long pid=strtoul(argv[2],&end,10);
    if(!pid || pid>2147483647 || !end || *end || geteuid()!=0) return 60;
    uint64_t prior=0;
    for(unsigned i=0;i<3;++i) {
        uint64_t sample=0;
        if(!check(unsigned(pid),sample) || (prior && sample<=prior)) { puts("replay-joint-ui-or-gpu-unready");return 66; }
        prior=sample;if(i<2) usleep(500000);
    }
    puts("replay-joint-ui-and-gpu-ready");return 0;
}
