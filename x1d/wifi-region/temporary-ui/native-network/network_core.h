#ifndef HBL_NETWORK_CORE_H
#define HBL_NETWORK_CORE_H
#include <stddef.h>

enum HblFile { HBL_DRIVER,HBL_BAND,HBL_INFRA,HBL_AP,HBL_UP,HBL_STARTED,
 HBL_WPA_CONF,HBL_WPA_PID,HBL_WPA_STAMP,HBL_BOUND,HBL_DNS,HBL_CLIENT_SOCKET,HBL_FILE_COUNT };
enum HblCommand { HBL_NM_ACTIVE,HBL_AP_ACTIVE,HBL_WIFI_POWER,HBL_IPV4_STATE,
 HBL_READ_BAND,HBL_READ_INFRA,HBL_READ_AP,HBL_READ_UP,
 HBL_DOWN,HBL_SET_AP,HBL_SET_INFRA,HBL_SET_BAND,HBL_SCAN_ENABLE,HBL_UP_RADIO,
 HBL_WPA_START,HBL_WPA_READY,HBL_WPA_STOP,HBL_DHCP,HBL_FLUSH,HBL_IFCONFIG,HBL_ROUTE };
enum HblResult { HBL_OK=0,HBL_FAILED=1,HBL_ARGUMENT=2,HBL_PID=4,HBL_UNSAFE=60,
 HBL_RUNTIME=61,HBL_LOCKED=62,HBL_INPUT=63,HBL_ROLLBACK=71 };
enum HblBegin {HBL_RESTORE=0,HBL_CALLBACK=1,HBL_RADIO_GUARDED=2};

/* Only fixed file/command identifiers cross the effect boundary. No pathname
 * or executable can be supplied through this API or either production CLI. */
typedef struct HblNetworkIO {
 void *context;
 int (*begin)(void *,int kind);
 int (*lease)(void *,int acquire);
 int (*finish)(void *);
 int (*read)(void *,enum HblFile,char *,size_t);
 int (*write)(void *,enum HblFile,const char *);
 int (*exists)(void *,enum HblFile);
 int (*remove)(void *,enum HblFile);
 int (*command)(void *,enum HblCommand,const char *,const char *,char *,size_t);
 int (*sleep_ms)(void *,unsigned);
} HblNetworkIO;

typedef struct HblLease {
 const char *interface_name,*ip,*subnet,*router,*dns;
} HblLease;

int hbl_network_run(HblNetworkIO *,const char *operation);
int hbl_dhcp_run(HblNetworkIO *,const char *event,const HblLease *);
int hbl_ipv4(const char *text,unsigned long *value);
int hbl_pid(const char *text,int *pid);
int hbl_wpa_argv(const char *bytes,size_t length);
int hbl_proc_stat(const char *bytes,int expected_pid,char *state,char *start,size_t capacity);
typedef struct HblWpaProcess {
 void *context;
 int (*probe)(void *,int pid,int check_arguments,char *state,char start[32]);
 int (*terminate)(void *,int pid);
 void (*pause)(void *,unsigned milliseconds);
} HblWpaProcess;
int hbl_stop_wpa(HblWpaProcess *,int pid,const char *saved_start);
const char *hbl_network_operation(int argc,char **argv);
const char *hbl_dhcp_event(int argc,char **argv);
#endif
