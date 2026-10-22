#define _GNU_SOURCE
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static volatile sig_atomic_t done;
static void stop(int n){(void)n;done=1;}
static void put(const char*p,const char*s){int f=open(p,O_WRONLY|O_CREAT|O_TRUNC,0600);if(f<0)_exit(119);if(write(f,s,strlen(s))!=(ssize_t)strlen(s))_exit(118);close(f);}
static void get(const char*p,char*b,size_t z){int f=open(p,O_RDONLY);ssize_t n=f<0?0:read(f,b,z-1);if(f>=0)close(f);if(n<0)n=0;b[n]=0;}
static int has(const char*p){return !access(p,F_OK);}
static void prop(const char*n,const char*v){char p[256];snprintf(p,sizeof p,"/tmp/props/%s",n);put(p,v);}
static void jobput(const char*s,const char*n,const char*v){char p[512];snprintf(p,sizeof p,"%s/%s",s,n);put(p,v);}
static int storage(void){
    prctl(PR_SET_NAME,"camera-storage",0,0,0);signal(SIGTERM,stop);char pid[32];snprintf(pid,sizeof pid,"%ld",(long)getpid());const char*s=getenv("X2D_MEMORY_CAPTURE_JOB");
    if(!s){put("/tmp/original.pid",pid);while(!done)usleep(10000);return 0;}
    put("/tmp/owned.pid",pid);put("/tmp/current-owned",s);char p[512];snprintf(p,sizeof p,"%s/storage.pid",s);char owner[32];get(p,owner,sizeof owner);if(atoi(owner)!=getpid())return 113;snprintf(p,sizeof p,"%s/storage.start",s);if(!has(p))return 114;
    char cfg[64];get("/tmp/scenario",cfg,sizeof cfg);const char*mapfile=!strcmp(cfg,"mapped-impostor")?"/system/bin/x2d-pixel-overclock/libx2d_private_memory_capture.so.extra":"/system/bin/x2d-pixel-overclock/libx2d_private_memory_capture.so";int f=open(mapfile,O_RDONLY);if(f<0||mmap(NULL,4096,PROT_READ,MAP_PRIVATE,f,0)==MAP_FAILED)return 115;close(f);jobput(s,"test.gate.identity.saved","1\n");jobput(s,"hook.ready","HOOK\n");
    while(!done){snprintf(p,sizeof p,"%s/capture.requested",s);if(has(p))jobput(s,"owner.ready","OWNER\n");usleep(10000);}return 0;
}
int main(int argc,char**argv){
    const char*base=strrchr(argv[0],'/');base=base?base+1:argv[0];char config[64];get("/tmp/scenario",config,sizeof config);
    if(!strcmp(base,"camera-storage"))return storage();
    if(!strcmp(base,"start")){if(argc!=2||strcmp(argv[1],"camera-storage"))return 116;if(!strcmp(config,"restore-service-failure"))return 17;pid_t p=fork();if(p<0)return 117;if(!p){int f=open("/dev/null",O_RDWR);dup2(f,0);dup2(f,1);dup2(f,2);if(f>2)close(f);unsetenv("LD_PRELOAD");unsetenv("X2D_MEMORY_CAPTURE_JOB");unsetenv("X2D_PRIVATE_CAPTURE_ABI");execl("/system/bin/camera-storage","/system/bin/camera-storage",NULL);_exit(127);}return 0;}
    if(!strcmp(base,"stop")){char b[32];get("/tmp/original.pid",b,sizeof b);if(atoi(b)>0)kill(atoi(b),SIGTERM);return 0;}
    if(!strcmp(base,"inputs")){if(argc!=4)return 112;put(argv[3],"100HASBL/A0000001\n");return 0;}
    if(!strcmp(base,"fix-3fr")||!strcmp(base,"pack"))return 0;
    if(strcmp(base,"dbus-send"))return 111;
    char job[256];get("/tmp/current-owned",job,sizeof job);
    for(int i=1;i<argc;i++){
        if(!strcmp(argv[i],"org.freedesktop.DBus.Properties.Get")){const char*n=argv[argc-1]+7;char p[512],v[64];if(!strcmp(n,"live_view_state")&&has("/tmp/live-pending")){char ticks[16];get("/tmp/live-pending",ticks,sizeof ticks);int k=atoi(ticks);if(k>0){snprintf(ticks,sizeof ticks,"%d",k-1);put("/tmp/live-pending",ticks);}else{unlink("/tmp/live-pending");prop("live_view_state","0");prop("stream_group","255");}}if(job[0]&&!strcmp(n,"capture_storage_mode")){snprintf(p,sizeof p,"%s/capture.requested",job);if(has(p))jobput(job,"owner.ready","OWNER\n");}snprintf(p,sizeof p,"/tmp/props/%s",n);get(p,v,sizeof v);printf("variant %s %s\n",!strcmp(n,"active_error")||!strcmp(n,"eshutter_current")?"boolean":"int32",v);return 0;}
        if(!strcmp(argv[i],"org.freedesktop.DBus.Properties.Set")){const char*n=argv[argc-2]+7;const char*v=strrchr(argv[argc-1],':')+1;if(!strcmp(config,"restore-setting-failure")&&!strcmp(n,"drive_mode")&&!strcmp(v,"1"))return 17;prop(n,v);return 0;}
        if(strstr(argv[i],".set_live_view")){if(!strcmp(config,"live-delayed")&&!strcmp(argv[argc-1],"boolean:false")){put("/tmp/live-pending","3");prop("stream_group","0");return 0;}prop("live_view_state",!strcmp(argv[argc-1],"boolean:true")?"1":"0");return 0;}
        if(strstr(argv[i],".do_exposure_extended2")){if(!job[0])return 110;jobput(job,"test.capture.called","1\n");int f=open("/tmp/exposure.count",O_WRONLY|O_CREAT|O_APPEND,0600);write(f,"1\n",2);close(f);if(!strcmp(config,"cancel-capture")){while(!has("/tmp/release-capture"))usleep(10000);jobput(job,"memory.consumers.drained","MEMORY_CONSUMERS_DRAINED_V1\n");}if(!strcmp(config,"live-restarted"))prop("live_view_state","1");jobput(job,"memory.inputs.ready","READY\n");return 0;}
        if(strstr(argv[i],".stop_session")){prop("exposure_status","0");return 0;}
    }return 109;
}
