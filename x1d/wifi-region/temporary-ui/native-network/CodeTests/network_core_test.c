#include "../network_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int assertions,cases,faults;
#define CHECK(x) do{assertions++;if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
typedef struct Mock {
 char data[HBL_FILE_COUNT][1024];int present[HBL_FILE_COUNT];
 int calls,fail_at,fail_command,fail_forever,mutations,marker_before_mutation;
 int operation_lock,lease_lock,radio_lock,foreign_radio,depth,callback[4],finish_count,begin_count;
 int unsafe,foreign_pid,alive,ready_after,ready_count,dhcp_no_bound,dhcp_fail_after_bound,foreign_lease;
 enum HblCommand commands[256];int count;
 char arguments[256][80];
} Mock;
static HblNetworkIO make_io(Mock *m);
static int fail(Mock *m){return ++m->calls==m->fail_at;}
static int begin(void *v,int kind){Mock *m=v;int callback=kind==HBL_CALLBACK;m->begin_count++;if(fail(m)||m->unsafe)return HBL_UNSAFE;
 if((kind==HBL_RADIO_GUARDED&&m->foreign_radio)||(callback?m->lease_lock:m->operation_lock))return HBL_LOCKED;
 m->callback[m->depth++]=callback;if(callback)m->lease_lock=1;else m->operation_lock=1;if(kind==HBL_RADIO_GUARDED)m->radio_lock=1;return 0;}
static int lease(void *v,int acquire){Mock *m=v;if(fail(m)||(acquire&&m->foreign_lease))return 1;if(acquire){if(m->lease_lock)return 0;m->lease_lock=1;}else m->lease_lock=0;return 0;}
static int finish(void *v){Mock *m=v;int bad=fail(m);m->finish_count++;CHECK(m->depth>0);int cb=m->callback[--m->depth];m->lease_lock=0;if(!cb)m->operation_lock=m->radio_lock=0;return bad;}
static int read_file(void *v,enum HblFile f,char *out,size_t cap){Mock *m=v;if(fail(m)||!m->present[f]||strlen(m->data[f])>=cap)return 1;strcpy(out,m->data[f]);return 0;}
static int write_file(void *v,enum HblFile f,const char *s){Mock *m=v;if(fail(m)||strlen(s)>=sizeof(m->data[f]))return 1;m->present[f]=1;strcpy(m->data[f],s);return 0;}
static int exists(void *v,enum HblFile f){Mock *m=v;return fail(m)?-1:m->present[f];}
static int remove_file(void *v,enum HblFile f){Mock *m=v;if(fail(m))return 1;m->present[f]=0;m->data[f][0]=0;return 0;}
static int pause_ms(void *v,unsigned ms){Mock *m=v;CHECK(ms==1000);return fail(m);}
static int execute(void *v,enum HblCommand c,const char *a,const char *b,char *out,size_t cap){
 Mock *m=v;CHECK(m->count<256);m->commands[m->count]=c;snprintf(m->arguments[m->count++],80,"%s|%s",a?a:"",b?b:"");
 if(fail(m)||(m->fail_forever&&(int)c==m->fail_command))return c==HBL_WPA_READY?-1:99;
 const char *text=0;
 switch(c){
 case HBL_NM_ACTIVE:case HBL_AP_ACTIVE:return 3;
 case HBL_WIFI_POWER:text="method return\n   boolean false\n";break;
 case HBL_IPV4_STATE:text="3: wlp1s0: <BROADCAST>\n";break;
 case HBL_READ_BAND:text="b\n";break;case HBL_READ_INFRA:text="0\n";break;case HBL_READ_AP:text="1\n";break;case HBL_READ_UP:text="1\n";break;
 case HBL_WPA_START:m->alive=1;m->present[HBL_WPA_PID]=m->present[HBL_WPA_STAMP]=1;strcpy(m->data[HBL_WPA_PID],"1234\n");strcpy(m->data[HBL_WPA_STAMP],"456");return 0;
 case HBL_WPA_READY:return m->ready_count++<m->ready_after?1:0;
 case HBL_WPA_STOP:if(m->foreign_pid)return 4;m->alive=0;return 0;
 case HBL_DHCP:{CHECK(!m->lease_lock);if(m->dhcp_no_bound)return 0;HblNetworkIO io=make_io(m);HblLease lease={"wlp1s0","192.0.2.25","255.255.255.0","192.0.2.1 192.0.2.2","192.0.2.53 198.51.100.53"};int rc=hbl_dhcp_run(&io,"bound",&lease);return m->dhcp_fail_after_bound?125:rc;}
 default:if(c>=HBL_DOWN){m->mutations++;if(!m->present[HBL_STARTED])m->marker_before_mutation=0;}return 0;
 }
 CHECK(out&&cap>strlen(text));strcpy(out,text);return 0;
}
static HblNetworkIO make_io(Mock *m){HblNetworkIO io={m,begin,lease,finish,read_file,write_file,exists,remove_file,execute,pause_ms};return io;}
static Mock fresh(void){Mock m={0};m.marker_before_mutation=1;m.fail_command=-1;m.present[HBL_DRIVER]=1;strcpy(m.data[HBL_DRIVER],"factory");return m;}
static void started(Mock *m){m->present[HBL_STARTED]=m->present[HBL_BAND]=m->present[HBL_INFRA]=m->present[HBL_AP]=m->present[HBL_UP]=1;strcpy(m->data[HBL_BAND],"b");strcpy(m->data[HBL_INFRA],"0");strcpy(m->data[HBL_AP],"1");strcpy(m->data[HBL_UP],"1");}
static void expected(Mock *m,const enum HblCommand *commands,size_t size){CHECK(m->count==(int)size);for(size_t n=0;n<size;n++)CHECK(m->commands[n]==commands[n]);}
static void parses(void){
 const char *good[]={"0.0.0.0","192.0.2.25","255.255.255.255","1.2.3.4"};
 const char *bad[]={"","1","1.2.3","1.2.3.4.5","256.0.0.1","01.2.3.4","-1.2.3.4","1.2.3.4;id","1.2.3.4\n","1.2.3.4 ","0x7f.0.0.1","1..3.4","-n"};
 for(size_t n=0;n<sizeof(good)/sizeof(good[0]);n++)CHECK(hbl_ipv4(good[n],0));for(size_t n=0;n<sizeof(bad)/sizeof(bad[0]);n++)CHECK(!hbl_ipv4(bad[n],0));
 int pid;CHECK(hbl_pid("1234\n",&pid)&&pid==1234);CHECK(hbl_pid("2147483647",&pid));
 const char *pids[]={"0","1","-1","+12","12x","12\n13","2147483648","42949672960","12\r\n"," 12"};for(size_t n=0;n<sizeof(pids)/sizeof(pids[0]);n++)CHECK(!hbl_pid(pids[n],&pid));
 static const char argv[]="/usr/sbin/wpa_supplicant\0-D\0nl80211\0-i\0wlp1s0\0-c\0/run/hbl-hotspot-ui/wpa.conf\0";
 CHECK(hbl_wpa_argv(argv,sizeof(argv)-1));CHECK(!hbl_wpa_argv(argv,sizeof(argv)-2));char changed[sizeof(argv)];memcpy(changed,argv,sizeof(argv));changed[1]='x';CHECK(!hbl_wpa_argv(changed,sizeof(argv)-1));
 char state,start[32];const char *stat="1234 (wpa ) odd) S 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 456 20\n";
 CHECK(hbl_proc_stat(stat,1234,&state,start,sizeof(start))&&state=='S'&&!strcmp(start,"456"));CHECK(!hbl_proc_stat(stat,1235,&state,start,sizeof(start)));CHECK(!hbl_proc_stat("1234 () S 1",1234,&state,start,sizeof(start)));
 char *cli[]={"network-native","start","/tmp/other"};CHECK(hbl_network_operation(2,cli));CHECK(!hbl_network_operation(3,cli));cli[1]="/bin/sh";CHECK(!hbl_network_operation(2,cli));cli[1]="renew";CHECK(hbl_dhcp_event(2,cli));cli[1]="arbitrary";CHECK(!hbl_dhcp_event(2,cli));cases++;
}
static void happy_paths(void){
 Mock m=fresh();HblNetworkIO io=make_io(&m);CHECK(hbl_network_run(&io,"start")==0);
 const enum HblCommand start_commands[]={HBL_NM_ACTIVE,HBL_AP_ACTIVE,HBL_WIFI_POWER,HBL_IPV4_STATE,HBL_READ_BAND,HBL_READ_INFRA,HBL_READ_AP,HBL_READ_UP,HBL_DOWN,HBL_SET_AP,HBL_SET_INFRA,HBL_SET_BAND,HBL_SCAN_ENABLE,HBL_UP_RADIO,HBL_WPA_START,HBL_WPA_READY};
 expected(&m,start_commands,sizeof(start_commands)/sizeof(start_commands[0]));CHECK(m.present[HBL_STARTED]&&m.alive&&m.marker_before_mutation);CHECK(!strcmp(m.data[HBL_WPA_CONF],"ctrl_interface=/run/hbl-hotspot-ui/ctrl\nupdate_config=0\n"));
 CHECK(!m.operation_lock&&!m.lease_lock);cases++;
 m.count=0;CHECK(hbl_network_run(&io,"dhcp")==0);CHECK(m.present[HBL_BOUND]&&!strcmp(m.data[HBL_BOUND],"ready"));CHECK(!strcmp(m.data[HBL_DNS],"192.0.2.53 198.51.100.53\n"));const enum HblCommand lease_commands[]={HBL_DHCP,HBL_IFCONFIG,HBL_ROUTE};expected(&m,lease_commands,3);CHECK(!strcmp(m.arguments[2],"192.0.2.1|"));cases++;
 m.count=0;CHECK(hbl_network_run(&io,"restore")==0);const enum HblCommand restore_commands[]={HBL_WPA_STOP,HBL_FLUSH,HBL_DOWN,HBL_SET_BAND,HBL_SET_INFRA,HBL_SET_AP,HBL_UP_RADIO};expected(&m,restore_commands,7);CHECK(!strcmp(m.arguments[3],"b|"));CHECK(!m.present[HBL_STARTED]&&!m.present[HBL_BOUND]&&!m.present[HBL_WPA_PID]&&!m.alive);cases++;
 m.count=0;CHECK(hbl_network_run(&io,"restore")==0);CHECK(m.count==1&&m.commands[0]==HBL_WPA_STOP);cases++;
}
static void failures(void){
 Mock ok=fresh();HblNetworkIO io=make_io(&ok);CHECK(!hbl_network_run(&io,"start"));int count=ok.calls;
 for(int step=1;step<=count;step++){Mock m=fresh();m.fail_at=step;io=make_io(&m);int rc=hbl_network_run(&io,"start");CHECK(rc!=0);CHECK(!m.depth&&!m.operation_lock&&!m.lease_lock);CHECK(m.marker_before_mutation);if(step<count&&rc!=HBL_ROLLBACK)CHECK(!m.present[HBL_STARTED]);faults++;}cases++;
 ok=fresh();started(&ok);io=make_io(&ok);CHECK(!hbl_network_run(&io,"restore"));count=ok.calls;
 for(int step=1;step<=count;step++){Mock m=fresh();started(&m);m.fail_at=step;io=make_io(&m);CHECK(hbl_network_run(&io,"restore")!=0);CHECK(!m.depth&&!m.operation_lock&&!m.lease_lock);faults++;}cases++;
 ok=fresh();started(&ok);io=make_io(&ok);CHECK(!hbl_network_run(&io,"dhcp"));count=ok.calls;
 for(int step=1;step<=count;step++){Mock m=fresh();started(&m);m.fail_at=step;io=make_io(&m);CHECK(hbl_network_run(&io,"dhcp")!=0);CHECK(!m.depth&&!m.operation_lock&&!m.lease_lock);faults++;}cases++;
 Mock m=fresh();m.ready_after=99;io=make_io(&m);CHECK(hbl_network_run(&io,"start")==HBL_FAILED);CHECK(m.ready_count==8&&!m.alive&&!m.present[HBL_STARTED]);cases++;
 m=fresh();m.ready_after=99;m.fail_forever=1;m.fail_command=HBL_FLUSH;io=make_io(&m);CHECK(hbl_network_run(&io,"start")==HBL_ROLLBACK);CHECK(m.present[HBL_STARTED]);cases++;
 m=fresh();started(&m);m.foreign_pid=1;io=make_io(&m);CHECK(hbl_network_run(&io,"restore")==HBL_PID);CHECK(m.present[HBL_STARTED]&&m.mutations==0);cases++;
 m=fresh();started(&m);strcpy(m.data[HBL_BAND],"b; id");io=make_io(&m);CHECK(hbl_network_run(&io,"restore")==HBL_INPUT);CHECK(m.count==0&&m.present[HBL_STARTED]);cases++;
 m=fresh();started(&m);m.present[HBL_BOUND]=1;m.dhcp_no_bound=1;io=make_io(&m);CHECK(hbl_network_run(&io,"dhcp")!=0&&!m.present[HBL_BOUND]);cases++;
 m=fresh();started(&m);m.dhcp_fail_after_bound=1;io=make_io(&m);CHECK(hbl_network_run(&io,"dhcp")!=0&&!m.present[HBL_BOUND]&&!m.present[HBL_DNS]);cases++;
 m=fresh();m.unsafe=1;io=make_io(&m);CHECK(hbl_network_run(&io,"start")==HBL_UNSAFE&&m.count==0);m.unsafe=0;m.operation_lock=1;CHECK(hbl_network_run(&io,"start")==HBL_LOCKED&&m.count==0);cases++;
 m=fresh();m.foreign_radio=1;io=make_io(&m);CHECK(hbl_network_run(&io,"start")==HBL_LOCKED&&m.count==0);CHECK(hbl_network_run(&io,"dhcp")==HBL_LOCKED&&m.count==0);cases++;
 m=fresh();started(&m);m.foreign_radio=1;io=make_io(&m);CHECK(hbl_network_run(&io,"restore")==0&&!m.radio_lock);cases++; /* radio-mode owns shared lock while invoking restore */
 m=fresh();started(&m);m.foreign_lease=1;io=make_io(&m);CHECK(hbl_network_run(&io,"restore")==HBL_LOCKED&&m.count==0&&m.present[HBL_STARTED]);cases++; /* unresolved/orphan callback is fail-closed */
}
static void leases(void){
 const HblLease good={"wlp1s0","192.0.2.25","255.255.255.0","192.0.2.1 192.0.2.2","192.0.2.53 198.51.100.53"};
 for(int which=0;which<8;which++){Mock m=fresh();started(&m);HblNetworkIO io=make_io(&m);HblLease input=good;
  switch(which){case 0:input.interface_name="eth0";break;case 1:input.ip="192.0.2.25;id";break;case 2:input.subnet="255.0.255.0";break;case 3:input.router="192.0.2.1 -n";break;case 4:input.dns="192.0.2.53\nsearch example";break;case 5:input.ip=0;break;case 6:input.router=0;break;case 7:input.dns=0;break;}
  CHECK(hbl_dhcp_run(&io,"bound",&input)==HBL_INPUT);CHECK(m.begin_count==0&&m.mutations==0);cases++;
 }
 Mock m=fresh();started(&m);HblNetworkIO io=make_io(&m);HblLease input=good;input.router="";input.dns="";CHECK(hbl_dhcp_run(&io,"renew",&input)==0);CHECK(m.count==1&&m.commands[0]==HBL_IFCONFIG);CHECK(!strcmp(m.data[HBL_DNS],"\n"));cases++;
 m=fresh();io=make_io(&m);input.ip=input.subnet=input.router=input.dns=0;CHECK(hbl_dhcp_run(&io,"deconfig",&input)==0&&m.begin_count==0);CHECK(hbl_dhcp_run(&io,"arbitrary",&good)==HBL_ARGUMENT&&m.begin_count==0);cases++;
 m=fresh();started(&m);m.fail_forever=1;m.fail_command=HBL_ROUTE;m.present[HBL_BOUND]=m.present[HBL_DNS]=1;io=make_io(&m);CHECK(hbl_dhcp_run(&io,"bound",&good)!=0);CHECK(!m.present[HBL_BOUND]&&!m.present[HBL_DNS]);cases++;
}
typedef struct ProcessMock {int probes,signals,pauses,foreign,gone,zombie,timeout,changed_before,changed_after;char start[32];} ProcessMock;
static int probe(void *v,int pid,int check,char *state,char start[32]){
 ProcessMock *p=v;CHECK(pid==1234);CHECK(check==(p->probes<2));p->probes++;
 if(p->foreign)return -1;if(p->gone)return 0;*state=p->zombie?'Z':'S';
 strcpy(start,(p->changed_before&&p->probes==2)||(p->changed_after&&p->signals)?"999":p->start);
 if(p->signals&&!p->timeout&&!p->changed_after)return 0;return 1;
}
static int terminate(void *v,int pid){ProcessMock *p=v;CHECK(pid==1234);p->signals++;return 0;}
static void pause_process(void *v,unsigned ms){ProcessMock *p=v;CHECK(ms==100);p->pauses++;}
static void processes(void){
 ProcessMock p={0};strcpy(p.start,"456");HblWpaProcess io={&p,probe,terminate,pause_process};
 CHECK(hbl_stop_wpa(&io,1234,0)==0&&p.signals==1);cases++; /* old shell PID, no wpa.start */
 p=(ProcessMock){0};strcpy(p.start,"456");CHECK(hbl_stop_wpa(&io,1234,"456")==0&&p.signals==1);cases++;
 p=(ProcessMock){0};strcpy(p.start,"999");CHECK(hbl_stop_wpa(&io,1234,"456")!=0&&p.signals==0);cases++;
 p=(ProcessMock){0};p.foreign=1;CHECK(hbl_stop_wpa(&io,1234,0)!=0&&p.signals==0);cases++;
 p=(ProcessMock){0};p.gone=1;CHECK(hbl_stop_wpa(&io,1234,0)==0&&p.signals==0);cases++;
 p=(ProcessMock){0};p.zombie=1;strcpy(p.start,"456");CHECK(hbl_stop_wpa(&io,1234,0)==0&&p.signals==0);cases++;
 p=(ProcessMock){0};p.timeout=1;strcpy(p.start,"456");CHECK(hbl_stop_wpa(&io,1234,0)!=0&&p.signals==1&&p.pauses==49);cases++;
 p=(ProcessMock){0};p.changed_before=1;strcpy(p.start,"456");CHECK(hbl_stop_wpa(&io,1234,0)!=0&&p.signals==0);cases++;
 p=(ProcessMock){0};p.changed_after=1;strcpy(p.start,"456");CHECK(hbl_stop_wpa(&io,1234,0)==0&&p.signals==1);cases++;
}
int main(void){parses();happy_paths();failures();leases();processes();printf("{\"passed\":true,\"cases\":%d,\"assertions\":%d,\"faultPoints\":%d,\"realCommands\":0,\"networkOperations\":0}\n",cases,assertions,faults);return 0;}
