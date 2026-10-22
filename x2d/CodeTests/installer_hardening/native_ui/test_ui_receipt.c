#define _GNU_SOURCE
#include "ui_receipt.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
static unsigned checks;
#define CK(x) do{++checks;if(!(x)){printf("UI_RECEIPT_FAIL line=%d checks=%u\n",__LINE__,checks);fflush(stdout);reboot(RB_POWER_OFF);_exit(1);}}while(0)
static const char*build="0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
static const char*gui="abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789";
static uint64_t now(void){struct timespec t;CK(!clock_gettime(CLOCK_MONOTONIC,&t));return (uint64_t)t.tv_sec*1000+t.tv_nsec/1000000;}
static void put_query(unsigned n){char s[64];int bytes=snprintf(s,sizeof s,"UI_QUERY_R1 %032x\n",n);CK(bytes==45);int f=open(X2D_UI_QUERY_PATH,O_CREAT|O_WRONLY|O_TRUNC,0600);CK(f>=0);CK(write(f,s,(size_t)bytes)==bytes);CK(!close(f));}
static X2dUiQuery get_query(X2dUiReceiptWorker*w){X2dUiQuery q;int got=0;for(unsigned i=0;i<1000&&!got;i++){got=x2d_ui_receipt_take_query(w,&q);if(!got)usleep(1000);}CK(got);return q;}
static X2dUiReceipt get_receipt(const char*nonce){X2dUiReceipt r;int got=0;for(unsigned i=0;i<1000&&!got;i++){char text[513];int f=open(X2D_UI_RECEIPT_PATH,O_RDONLY);if(f>=0){ssize_t n=read(f,text,sizeof text);close(f);got=n>0&&x2d_ui_receipt_parse(text,(size_t)n,&r)&&!strcmp(r.nonce,nonce);}if(!got)usleep(1000);}CK(got);return r;}
int main(void){setvbuf(stdout,NULL,_IONBF,0);CK(!mount("proc","/proc","proc",0,NULL));CK(!mkdir("/tmp",0700));
 X2dUiReceipt input={.pid=1,.start_ticks=1,.generation=1,.sampled_ms=1,.capabilities=4095,.state=X2D_UI_READY},out={0};memset(input.nonce,'0',32);memcpy(input.build,build,65);memcpy(input.gui,gui,65);char text[513];size_t n=x2d_ui_receipt_format(&input,text,sizeof text);CK(n>0&&x2d_ui_receipt_parse(text,n,&out));CK(!memcmp(&input,&out,sizeof input));
 for(size_t i=0;i<n;i++)CK(!x2d_ui_receipt_parse(text,i,&out));
 for(size_t i=0;i<n;i++){char prior=text[i];text[i]=0;CK(!x2d_ui_receipt_parse(text,n,&out));text[i]=prior;}
 CK(x2d_ui_receipt_ready(&input,input.nonce,1,1,build,gui,4095,1,1));CK(!x2d_ui_receipt_ready(&input,"00000000000000000000000000000001",1,1,build,gui,1,1,1));CK(!x2d_ui_receipt_ready(&input,input.nonce,2,1,build,gui,1,1,1));CK(!x2d_ui_receipt_ready(&input,input.nonce,1,2,build,gui,1,1,1));CK(!x2d_ui_receipt_ready(&input,input.nonce,1,1,gui,gui,1,1,1));CK(!x2d_ui_receipt_ready(&input,input.nonce,1,1,build,gui,8192,1,1));CK(!x2d_ui_receipt_ready(&input,input.nonce,1,1,build,gui,1,2,2));CK(!x2d_ui_receipt_ready(&input,input.nonce,1,1,build,gui,1,1,2002));
 CK(!x2d_ui_receipt_create("short",gui));X2dUiReceiptWorker*w=x2d_ui_receipt_create(build,gui);CK(w);X2dUiQuery q;CK(!x2d_ui_receipt_take_query(w,&q));
 for(unsigned round=1;round<=64;round++){put_query(round);uint64_t request=now();q=get_query(w);CK(!x2d_ui_receipt_take_query(w,&q));if(round%2){x2d_ui_receipt_invalidate(w);CK(!x2d_ui_receipt_reply(w,&q,X2D_UI_READY,4095,now()));}else CK(x2d_ui_receipt_reply(w,&q,X2D_UI_READY,4095,now()));CK(!x2d_ui_receipt_reply(w,&q,X2D_UI_READY,4095,now()));X2dUiReceipt r=get_receipt(q.nonce);CK(r.pid==(uint64_t)getpid()&&r.start_ticks>0);CK(r.state==(round%2?X2D_UI_STOPPED:X2D_UI_READY));CK(x2d_ui_receipt_ready(&r,q.nonce,r.pid,r.start_ticks,build,gui,4095,request,now())==(round%2==0));}
 // A newer query must not receive a queued reply carrying the old nonce.
 put_query(65);q=get_query(w);put_query(66);CK(x2d_ui_receipt_reply(w,&q,X2D_UI_READY,4095,now()));q=get_query(w);CK(!strcmp(q.nonce,"00000000000000000000000000000042"));CK(x2d_ui_receipt_reply(w,&q,X2D_UI_READY,4095,now()));get_receipt(q.nonce);
 // Reject a link rather than overwriting/following its target.
 CK(!unlink(X2D_UI_RECEIPT_PATH));CK(!symlink("/tmp/untouched",X2D_UI_RECEIPT_PATH));put_query(67);q=get_query(w);CK(x2d_ui_receipt_reply(w,&q,X2D_UI_READY,4095,now()));usleep(40000);struct stat st;CK(!lstat(X2D_UI_RECEIPT_PATH,&st)&&S_ISLNK(st.st_mode));CK(access("/tmp/untouched",F_OK));
 x2d_ui_receipt_stop(w);CK(!x2d_ui_receipt_take_query(w,&q));int released=0;for(unsigned i=0;i<1000&&!released;i++){released=x2d_ui_receipt_release(w);if(!released)usleep(1000);}CK(released);printf("UI_RECEIPT_PASS assertions=%u replies=66 camera_operations=0\n",checks);reboot(RB_POWER_OFF);return 0;}
