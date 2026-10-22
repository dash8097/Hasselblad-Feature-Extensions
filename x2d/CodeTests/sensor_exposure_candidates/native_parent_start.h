/* Start the fixed installed component as a normal child of camera-service.
 * No credentials, capabilities, SELinux policy or process labels are changed.
 * The parent never maps hardware or emits a pulse. */
#ifndef X2D_NATIVE_PARENT_START_H
#define X2D_NATIVE_PARENT_START_H
extern void*memcpy(void*,const void*,size_t);
extern int memcmp(const void*,const void*,size_t);
extern long syscall(long,...);
extern int execve(const char*,char*const[],char*const[]);
extern int snprintf(char*,size_t,const char*,...);
static int native_camera_parent(void){
 const char*flag=getenv("X2D_ESFLASH_CAMERA_CHILD");if(!flag||!equal(flag,"1")||getuid()!=0)return 0;
 static const char expected[]="/system/bin/camera-service";
 char exe[64],args[64];long n=readlink("/proc/self/exe",exe,sizeof exe);
 if(n!=(long)sizeof(expected)-1||memcmp(exe,expected,sizeof(expected)-1))return 0;
 int fd=open("/proc/self/cmdline",0x80000);if(fd<0)return 0;
 n=read(fd,args,sizeof args);int closed=close(fd);
 return !closed&&n==(long)sizeof expected&&!memcmp(args,expected,sizeof expected);
}
/* The installed path is fixed, opened once without following symlinks. The
 * child executes that same descriptor, preventing a path replacement race. */
static int native_open_child_image(void){
 int fd=open("/system/bin/x2d-esflash",0xa0000);if(fd<0)return -1;
 uint64_t st[16]={0};unsigned char elf[20];int ok=syscall(80,fd,st)==0;
 uint32_t mode=0,uid=1,links=0;
 if(ok){memcpy(&mode,(char*)st+16,4);memcpy(&links,(char*)st+20,4);memcpy(&uid,(char*)st+24,4);}
 ok=ok&&(mode&0170000)==0100000&&!(mode&022)&&uid==0&&links==1&&read(fd,elf,sizeof elf)==sizeof elf&&
  !memcmp(elf,"\177ELF\002\001\001",7)&&(elf[16]==2||elf[16]==3)&&!elf[17]&&elf[18]==183&&!elf[19];
 if(!ok){close(fd);return -1;}return fd;
}
static int native_close_child_handles(int keep){
 int dir=open("/proc/self/fd",0x84000);if(dir<0)return 0;unsigned char buffer[4096];
 for(;;){long n=syscall(61,dir,buffer,(unsigned long)sizeof buffer);if(n<0){close(dir);return 0;}if(!n)break;
  for(long at=0;at<n;){if(n-at<20){close(dir);return 0;}unsigned len=buffer[at+16]|((unsigned)buffer[at+17]<<8);
   if(len<20||len>(unsigned)(n-at)){close(dir);return 0;}unsigned value=0,used=0;int numeric=1;
   for(;used<len-19&&buffer[at+19+used];++used){unsigned digit=buffer[at+19+used]-'0';if(digit>9||value>214748364u||(value==214748364u&&digit>7)){numeric=0;break;}value=value*10+digit;}
   if(used==len-19){close(dir);return 0;}if(numeric&&used&&value>2&&(int)value!=dir&&(int)value!=keep)close((int)value);
   at+=len;
  }
 }close(dir);return 1;
}
static void native_start_camera_child(void){
 if(!native_camera_parent())return;
 /* Bind to the official 4.2.0 service before creating any child. */
 static const unsigned char expected[32]={0xfb,0xcf,0x82,0x8f,0x73,0xbc,0xa1,0x3f,0x0c,0x8b,0x95,0xe7,0xdd,0x0b,0x95,0xac,0x48,0x3a,0xe3,0x69,0x54,0xec,0x06,0x17,0x90,0x98,0xc8,0xa1,0xa6,0x5f,0x9f,0x82};
 int image=open("/system/bin/camera-service",0xa0000);if(image<0)return;
 long size=lseek(image,0,2);if(size<0x100000||size>0x4000000){close(image);return;}
 void*data=mmap(0,(unsigned long)size,1,2,image,0);close(image);if(data==(void*)-1)return;
 unsigned char digest[32];int valid=SHA256(data,(unsigned long)size,digest)&&same_bytes(digest,expected,32);munmap(data,(unsigned long)size);if(!valid)return;
 int child_image=native_open_child_image();if(child_image<0)return;
 char executable[64];int length=snprintf(executable,sizeof executable,"/proc/self/fd/%d",child_image);
 if(length<=0||length>=(int)sizeof executable){close(child_image);return;}
 int child=fork();if(!child){
  int detached=fork();if(detached<0)_exit(120);if(detached)_exit(0);
  if(!native_close_child_handles(child_image))_exit(121);
  char*args[]={(char*)"/system/bin/x2d-esflash",0};
  char*environment[]={(char*)"PATH=/system/bin:/system/xbin",0};
  execve(executable,args,environment);_exit(122);
 }
 close(child_image);if(child<0)return;
 // Only our own intermediate child is reaped. Never alter SIGCHLD handling
 // or wait for the persistent worker on the factory startup thread.
 int status=0;for(unsigned i=0;i<40;++i){int result=waitpid(child,&status,1);if(result!=0)break;poll(0,0,5);}
}
#endif
