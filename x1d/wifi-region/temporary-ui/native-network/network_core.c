#include "network_core.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int space(char c){return c==' '||c=='\t'||c=='\n';}
static void trim(char *s){size_t n=strlen(s);while(n&&(s[n-1]=='\n'||s[n-1]=='\r'||s[n-1]==' '||s[n-1]=='\t'))s[--n]=0;}
static int same(const char *a,const char *b){return a&&b&&!strcmp(a,b);}
int hbl_ipv4(const char *s,unsigned long *value){
 unsigned long n=0;if(!s||!*s)return 0;
 for(int part=0;part<4;part++){
  unsigned v=0,digits=0;if(*s<'0'||*s>'9')return 0;
  const int zero=*s=='0';
  while(*s>='0'&&*s<='9'){if(++digits>3)return 0;v=v*10+(unsigned)(*s++-'0');}
  if(v>255||(zero&&digits>1))return 0;n=(n<<8)|v;
  if(part==3){if(*s)return 0;}else if(*s++!='.')return 0;
 }
 if(value)*value=n;return 1;
}
int hbl_pid(const char *s,int *pid){
 unsigned long v=0;size_t n=0;if(!s||!*s)return 0;
 while(*s>='0'&&*s<='9'){if(++n>10)return 0;v=v*10+(unsigned)(*s++-'0');if(v>INT_MAX)return 0;}
 if((*s&&strcmp(s,"\n"))||v<2)return 0;if(pid)*pid=(int)v;return 1;
}
int hbl_wpa_argv(const char *bytes,size_t n){
 static const char expected[]="/usr/sbin/wpa_supplicant\0-D\0nl80211\0-i\0wlp1s0\0-c\0/run/hbl-hotspot-ui/wpa.conf\0";
 return n==sizeof(expected)-1&&!memcmp(bytes,expected,n);
}
int hbl_proc_stat(const char *s,int expected_pid,char *state,char *start,size_t cap){
 /* comm may contain spaces or ')'; fields after its last ')' have a fixed
  * position. starttime is field 22 and must be decimal, never a shell token. */
 char *end;long pid;if(!s||!start||cap<2)return 0;
 pid=strtol(s,&end,10);if(pid!=expected_pid||end==s||*end!=' '||end[1]!='(')return 0;
 const char *tail=strrchr(end,')');if(!tail||tail[1]!=' '||!tail[2]||tail[3]!=' ')return 0;
 *state=tail[2];tail+=4;
 for(int field=4;field<=22;field++){
  const char *token=tail;while(*tail&&*tail!=' '&&*tail!='\n')tail++;
  if(tail==token)return 0;
  if(field==22){size_t size=(size_t)(tail-token);if(size>=cap)return 0;for(size_t i=0;i<size;i++)if(token[i]<'0'||token[i]>'9')return 0;memcpy(start,token,size);start[size]=0;return 1;}
  if(*tail!=' ')return 0;while(*tail==' ')tail++;
 }return 0;
}
int hbl_stop_wpa(HblWpaProcess *io,int pid,const char *saved){
 char state,actual[32],expected[32];if(pid<2)return 1;
 int live=io->probe(io->context,pid,1,&state,actual);if(live<=0)return live?1:0;if(state=='Z')return 0;
 if(saved&&*saved&&strcmp(saved,actual))return 1;strcpy(expected,actual);
 live=io->probe(io->context,pid,1,&state,actual);if(live<=0)return live?1:0;
 if(strcmp(expected,actual)||io->terminate(io->context,pid))return 1;
 for(int n=0;n<50;n++){
  live=io->probe(io->context,pid,0,&state,actual);if(live<0)return 1;
  if(!live||state=='Z'||strcmp(expected,actual))return 0;
  if(n!=49)io->pause(io->context,100);
 }return 1;
}
static int addresses(const char *s,char first[16]){
 unsigned count=0;if(!s||strlen(s)>512)return 0;if(first)first[0]=0;
 while(*s){while(space(*s))s++;if(!*s)break;char token[16];size_t n=0;
  while(*s&&!space(*s)){if(n>=15)return 0;token[n++]=*s++;}token[n]=0;
  if(!hbl_ipv4(token,0)||++count>16)return 0;if(first&&count==1)memcpy(first,token,n+1);
 }return 1;
}
static int band(const char *s){return same(s,"auto")||same(s,"a")||same(s,"b");}
static int bit(const char *s){return same(s,"0")||same(s,"1");}
static int command(HblNetworkIO *io,enum HblCommand c,const char *a,const char *b){return io->command(io->context,c,a,b,0,0);}
static int get(HblNetworkIO *io,enum HblFile f,char *out,size_t cap){if(io->read(io->context,f,out,cap))return 0;trim(out);return 1;}
static int power_off(const char *reply){
 int found=0;while(*reply){const char *end=strchr(reply,'\n');size_t n=end?(size_t)(end-reply):strlen(reply);
  while(n&&(*reply==' '||*reply=='\t')){reply++;n--;}
  if(n==13&&!memcmp(reply,"boolean false",13)){if(found++)return 0;}
  else if(n>=7&&!memcmp(reply,"boolean",7))return 0;
  if(!end)break;reply=end+1;
 }return found==1;
}
static int cleanup(HblNetworkIO *io){
 static const enum HblFile files[]={HBL_WPA_CONF,HBL_WPA_PID,HBL_WPA_STAMP,HBL_BOUND,HBL_DNS,HBL_CLIENT_SOCKET,HBL_STARTED};
 for(size_t n=0;n<sizeof(files)/sizeof(files[0]);n++)if(io->remove(io->context,files[n]))return HBL_FAILED;
 return HBL_OK;
}
static int restore(HblNetworkIO *io){
 char oldband[32],infra[16],ap[16],up[16];int started=io->exists(io->context,HBL_STARTED);
 if(started<0)return HBL_UNSAFE;
 if(started&&(!get(io,HBL_BAND,oldband,sizeof(oldband))||!band(oldband)||
   !get(io,HBL_INFRA,infra,sizeof(infra))||!bit(infra)||!get(io,HBL_AP,ap,sizeof(ap))||!bit(ap)||
   !get(io,HBL_UP,up,sizeof(up))||!bit(up)))return HBL_INPUT;
 if(command(io,HBL_WPA_STOP,0,0))return HBL_PID;
 if(started&&(command(io,HBL_FLUSH,0,0)||command(io,HBL_DOWN,0,0)||command(io,HBL_SET_BAND,oldband,0)||
   command(io,HBL_SET_INFRA,infra,0)||command(io,HBL_SET_AP,ap,0)||(same(up,"1")&&command(io,HBL_UP_RADIO,0,0))))return HBL_FAILED;
 return cleanup(io);
}
static int start(HblNetworkIO *io){
 char output[8192],saved[4][32];const enum HblCommand reads[]={HBL_READ_BAND,HBL_READ_INFRA,HBL_READ_AP,HBL_READ_UP};
 const enum HblFile files[]={HBL_BAND,HBL_INFRA,HBL_AP,HBL_UP};
 if(!get(io,HBL_DRIVER,output,sizeof(output))||!same(output,"factory"))return HBL_INPUT;
 if(command(io,HBL_NM_ACTIVE,0,0)!=3||command(io,HBL_AP_ACTIVE,0,0)!=3)return HBL_FAILED;
 if(io->command(io->context,HBL_WIFI_POWER,0,0,output,sizeof(output))||!power_off(output))return HBL_FAILED;
 if(io->exists(io->context,HBL_STARTED)!=0||io->exists(io->context,HBL_WPA_PID)!=0)return HBL_UNSAFE;
 if(io->command(io->context,HBL_IPV4_STATE,0,0,output,sizeof(output))||strstr(output,"inet "))return HBL_FAILED;
 for(int n=0;n<4;n++){
  if(io->command(io->context,reads[n],0,0,saved[n],sizeof(saved[n])))return HBL_FAILED;trim(saved[n]);
  if(n?!bit(saved[n]):!band(saved[n]))return HBL_INPUT;
 }
 for(int n=0;n<4;n++)if(io->write(io->context,files[n],saved[n]))return HBL_FAILED;
 if(io->write(io->context,HBL_STARTED,""))return HBL_FAILED;
 int failed=command(io,HBL_DOWN,0,0)||command(io,HBL_SET_AP,"0",0)||command(io,HBL_SET_INFRA,"1",0)||
  command(io,HBL_SET_BAND,"auto",0)||command(io,HBL_SCAN_ENABLE,0,0)||command(io,HBL_UP_RADIO,0,0)||
  io->write(io->context,HBL_WPA_CONF,"ctrl_interface=/run/hbl-hotspot-ui/ctrl\nupdate_config=0\n")||command(io,HBL_WPA_START,0,0);
 if(!failed){for(int n=0;n<8;n++){
   int ready=command(io,HBL_WPA_READY,0,0);if(!ready)return HBL_OK;
   if(ready<0||n==7||io->sleep_ms(io->context,1000)){failed=1;break;}
 }}
 if(failed)return restore(io)==HBL_OK?HBL_FAILED:HBL_ROLLBACK;
 return HBL_FAILED;
}
const char *hbl_network_operation(int argc,char **argv){return argc==2&&(same(argv[1],"start")||same(argv[1],"dhcp")||same(argv[1],"restore"))?argv[1]:0;}
const char *hbl_dhcp_event(int argc,char **argv){return argc==2&&(same(argv[1],"bound")||same(argv[1],"renew")||same(argv[1],"deconfig")||same(argv[1],"nak")||same(argv[1],"leasefail"))?argv[1]:0;}
int hbl_network_run(HblNetworkIO *io,const char *op){
 if(!same(op,"start")&&!same(op,"dhcp")&&!same(op,"restore"))return HBL_ARGUMENT;
 int rc=io->begin(io->context,same(op,"restore")?HBL_RESTORE:HBL_RADIO_GUARDED);if(rc)return rc;
 if(io->lease(io->context,1))rc=HBL_LOCKED;
 else if(same(op,"start"))rc=start(io);
 else if(same(op,"restore"))rc=restore(io);
 else{
  if(io->exists(io->context,HBL_STARTED)!=1||io->remove(io->context,HBL_BOUND)||io->remove(io->context,HBL_DNS))rc=HBL_FAILED;
  else if(io->lease(io->context,0))rc=HBL_LOCKED;
  else if(command(io,HBL_DHCP,0,0)){
   rc=HBL_FAILED;
   /* The owned foreground client has been joined before this branch. Take
    * the lease lock again before invalidating a callback's partial success. */
   if(!io->lease(io->context,1)){io->remove(io->context,HBL_BOUND);io->remove(io->context,HBL_DNS);}
  }
  else if(io->lease(io->context,1))rc=HBL_LOCKED;
  else rc=io->exists(io->context,HBL_BOUND)==1?HBL_OK:HBL_FAILED;
 }
 if(io->finish(io->context)&&!rc)rc=HBL_RUNTIME;return rc;
}
int hbl_dhcp_run(HblNetworkIO *io,const char *event,const HblLease *lease){
 if(!same(event,"bound")&&!same(event,"renew")&&!same(event,"deconfig")&&!same(event,"nak")&&!same(event,"leasefail"))return HBL_ARGUMENT;
 if(!lease||!same(lease->interface_name,"wlp1s0"))return HBL_INPUT;
 if(!same(event,"bound")&&!same(event,"renew"))return HBL_OK;
 unsigned long mask;char router[16];
 if(!hbl_ipv4(lease->ip,0)||!hbl_ipv4(lease->subnet,&mask)||((~mask&0xffffffffUL)&((~mask&0xffffffffUL)+1UL))||
   !addresses(lease->router,router)||!addresses(lease->dns,0))return HBL_INPUT;
 int rc=io->begin(io->context,HBL_CALLBACK);if(rc)return rc;
 if(io->exists(io->context,HBL_STARTED)!=1||io->remove(io->context,HBL_BOUND)||io->remove(io->context,HBL_DNS))rc=HBL_FAILED;
 else if(command(io,HBL_IFCONFIG,lease->ip,lease->subnet)||(router[0]&&command(io,HBL_ROUTE,router,0)))rc=HBL_FAILED;
 else{
  char dns[514];snprintf(dns,sizeof(dns),"%s\n",lease->dns);
  if(io->write(io->context,HBL_DNS,dns)||io->write(io->context,HBL_BOUND,"ready"))rc=HBL_FAILED;
 }
 if(rc){io->remove(io->context,HBL_BOUND);io->remove(io->context,HBL_DNS);}
 if(io->finish(io->context)&&!rc)rc=HBL_RUNTIME;return rc;
}
