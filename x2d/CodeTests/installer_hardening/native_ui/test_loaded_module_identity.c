#define _GNU_SOURCE
#include "loaded_module_identity.h"
#include "ui_receipt.h"
#include "receipt_sha256.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <time.h>
uintptr_t x2d_identity_test_address;
static int checks;static const char*path="/system/lib64/libx2d_native_menu.so";
static void ck(int ok){++checks;if(!ok){printf("LOADED_IDENTITY_FAIL %d\n",checks);reboot(RB_POWER_OFF);_exit(1);}}
static void save(const char*p,const void*data,size_t n){int fd=open(p,O_WRONLY|O_CREAT|O_TRUNC,0644);ck(fd>=0&&write(fd,data,n)==(ssize_t)n);ck(close(fd)==0);}
static void pause_ms(void){struct timespec p={0,10000000};nanosleep(&p,NULL);}
static void release(X2dUiReceiptWorker*w){x2d_ui_receipt_stop(w);int done=0;for(unsigned i=0;i<500&&!done;i++){done=x2d_ui_receipt_release(w);if(!done)pause_ms();}ck(done);}
int main(){mount("devtmpfs","/dev","devtmpfs",0,NULL);int console=open("/dev/console",O_RDWR);if(console>=0){dup2(console,0);dup2(console,1);dup2(console,2);if(console>2)close(console);}setbuf(stdout,NULL);ck(mount("proc","/proc","proc",0,NULL)==0);ck(!mkdir("/tmp",01777)&&!mkdir("/system",0755)&&!mkdir("/system/lib64",0755));
 unsigned char bytes[8192];for(unsigned i=0;i<sizeof bytes;i++)bytes[i]=(unsigned char)(i*13);save(path,bytes,sizeof bytes);
 int fd=open(path,O_RDONLY);ck(fd>=0);void*address=mmap(NULL,sizeof bytes,PROT_READ|PROT_EXEC,MAP_PRIVATE,fd,0);ck(address!=MAP_FAILED);close(fd);x2d_identity_test_address=(uintptr_t)address+16;
 char hash[65],expected[65];unsigned char digest[32];ReceiptSha h;rsha_init(&h);rsha_update(&h,bytes,sizeof bytes);rsha_final(&h,digest);for(unsigned i=0;i<32;i++)sprintf(expected+i*2,"%02x",digest[i]);ck(x2d_loaded_module_hash(hash)&&!strcmp(hash,expected));
 uintptr_t good=x2d_identity_test_address;x2d_identity_test_address=(uintptr_t)&main;ck(!x2d_loaded_module_hash(hash)&&!hash[0]);x2d_identity_test_address=good;
 ck(!mprotect(address,sizeof bytes,PROT_READ));ck(!x2d_loaded_module_hash(hash));ck(!mprotect(address,sizeof bytes,PROT_READ|PROT_EXEC));ck(x2d_loaded_module_hash(hash));
 ck(!chmod(path,0666));ck(!x2d_loaded_module_hash(hash));ck(!chmod(path,0644));
 ck(!link(path,"/tmp/linked"));ck(!x2d_loaded_module_hash(hash));ck(!unlink("/tmp/linked"));
 ck(!rename(path,"/system/lib64/old.so"));save(path,bytes,sizeof bytes);ck(!x2d_loaded_module_hash(hash));ck(!unlink(path)&&!rename("/system/lib64/old.so",path));ck(x2d_loaded_module_hash(hash));
 ck(!rename(path,"/system/lib64/old.so")&&!symlink("old.so",path));ck(!x2d_loaded_module_hash(hash));ck(!unlink(path)&&!rename("/system/lib64/old.so",path));
 ck(!rename("/system/lib64","/system/real")&&!symlink("real","/system/lib64"));ck(!x2d_loaded_module_hash(hash));ck(!unlink("/system/lib64")&&!rename("/system/real","/system/lib64"));ck(x2d_loaded_module_hash(hash));
 const char*gui="bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";X2dUiReceiptWorker*w=x2d_ui_receipt_create_loaded(gui);ck(w!=NULL);
 save(X2D_UI_QUERY_PATH,"UI_QUERY_R1 11111111111111111111111111111111\n",45);X2dUiQuery q;int got=0;for(unsigned i=0;i<500&&!got;i++){got=x2d_ui_receipt_take_query(w,&q);if(!got)pause_ms();}ck(got);ck(x2d_ui_receipt_reply(w,&q,X2D_UI_READY,127,1000));
 X2dUiReceipt r;int parsed=0;for(unsigned i=0;i<500&&!parsed;i++){char text[512];int f=open(X2D_UI_RECEIPT_PATH,O_RDONLY);if(f>=0){ssize_t n=read(f,text,sizeof text);close(f);if(n>0)parsed=x2d_ui_receipt_parse(text,(size_t)n,&r);}if(!parsed)pause_ms();}ck(parsed&&!strcmp(r.build,expected)&&!strcmp(r.gui,gui)&&r.state==X2D_UI_READY);
 save(X2D_UI_QUERY_PATH,"UI_QUERY_R1 22222222222222222222222222222222\n",45);got=0;for(unsigned i=0;i<500&&!got;i++){got=x2d_ui_receipt_take_query(w,&q);if(!got)pause_ms();}ck(got);
 bytes[500]^=1;save(path,bytes,sizeof bytes);ck(x2d_ui_receipt_reply(w,&q,X2D_UI_READY,127,1100));for(unsigned i=0;i<30;i++)pause_ms();char text[512];fd=open(X2D_UI_RECEIPT_PATH,O_RDONLY);ck(fd>=0);ssize_t n=read(fd,text,sizeof text);close(fd);ck(n>0&&x2d_ui_receipt_parse(text,(size_t)n,&r)&&!strcmp(r.nonce,"11111111111111111111111111111111"));release(w);
 ck(!munmap(address,sizeof bytes));w=x2d_ui_receipt_create_loaded(gui);ck(w!=NULL);for(unsigned i=0;i<30;i++)pause_ms();ck(!x2d_ui_receipt_take_query(w,&q));release(w);
 printf("LOADED_IDENTITY_PASS assertions=%d actual_ARM_Linux_maps_files_worker=1 mapped_address_fixture=1\n",checks);reboot(RB_POWER_OFF);return 0;
}
