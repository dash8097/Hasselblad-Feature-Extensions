/* QEMU 私有假编码器/音频宿主；不模拟待测 POSIX adapter。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
static int writefile(const char*p,const char*s){int f=open(p,O_WRONLY|O_CREAT|O_TRUNC,0600);if(f<0)return 0;int ok=write(f,s,strlen(s))==(ssize_t)strlen(s);close(f);return ok;}
int main(int argc,char**argv){
 if(strstr(argv[0],"/sleep")){
  char text[64]={0};FILE*f=fopen("/tmp/x2d-shutter-audio.pid","r");if(!f)return 91;fgets(text,sizeof(text),f);fclose(f);
  if(argc!=2||strcmp(argv[1],"2147483647")||atol(text)!=(long)getpid())return 92;
  const char*a=getenv("X2D_AUDIO_SERVICE"),*l=getenv("LD_PRELOAD");
  if(!a||strcmp(a,"1")||!l||strcmp(l,"/system/lib64/libx2d_audio_preview.so"))return 93;
  return writefile("/tmp/audio-result","AUDIO_EXEC_PID_ENV_VERIFIED\n")?0:94;
 }
 if(argc!=2)return 95;
 char mode[64]="ok",p[512];FILE*f=fopen("/tmp/encoder-mode","r");if(f){fgets(mode,sizeof(mode),f);fclose(f);}
 snprintf(p,sizeof(p),"%s/encoder.started",argv[1]);writefile(p,"STARTED\n");
 if(!strcmp(mode,"wait"))usleep(800000);
 if(!strcmp(mode,"exit17"))return 17;
 if(!strcmp(mode,"error"))puts("x265 [error] synthetic failure");else puts("x265 info synthetic encoder");
 if(strcmp(mode,"missing")){
  snprintf(p,sizeof(p),"%s/heif.ready",argv[1]);
  if(!writefile(p,!strcmp(mode,"bad")?"HEIF_ENCODED 1 1 15\n":"HEIF_ENCODED 23326 17498 15\n"))return 96;
 }
 snprintf(p,sizeof(p),"%s/encoder.finished",argv[1]);writefile(p,"FINISHED\n");
 return 0;
}
