/* 正式服务的实际进程身份；不接受改argv0或旧true试验宿主。 */
#ifndef X2D_NATIVE_HOST_IDENTITY_H
#define X2D_NATIVE_HOST_IDENTITY_H
extern long readlink(const char *,char *,unsigned long);
extern int getuid(void);
static int native_selected_host(void){
 static const char expected[]="/system/bin/x2d-esflash";
 char exe[64],args[64];
 if(getuid()!=0)return 0;
 long n=readlink("/proc/self/exe",exe,sizeof exe);
 if(n!=(long)sizeof(expected)-1)return 0;
 for(unsigned long i=0;i<sizeof(expected)-1;i++)if(exe[i]!=expected[i])return 0;
 int fd=open("/proc/self/cmdline",0);
 if(fd<0)return 0;
 n=read(fd,args,sizeof args);int closed=close(fd);
 if(closed||n!=(long)sizeof expected)return 0;
 for(unsigned long i=0;i<sizeof expected;i++)if(args[i]!=expected[i])return 0;
 return 1;
}
#endif
