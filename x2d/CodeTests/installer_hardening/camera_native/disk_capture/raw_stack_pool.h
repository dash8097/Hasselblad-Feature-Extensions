/* RAW 堆栈独占自己的新池启动；借用已启动池时只归还引用。 */
#ifndef RAW_STACK_POOL_H
#define RAW_STACK_POOL_H
#include <stdint.h>
typedef struct {
 int (*open)(const char *,void *,void **);
 int (*state)(void *,uint8_t *);
 int (*start)(void *,void *);
 int (*stop)(void *,void *);
 int (*close)(void *);
 int (*attach)(const char *,void **);
 int (*detach)(void *);
} RsPoolApi;
static inline int rs_pool_acquire(RsPoolApi api,void **device,void **module,int *owned,int *open_rc){
 if(!device||*device||!module||*module||!owned||!open_rc||!api.open||!api.state||!api.start||!api.stop||!api.close||!api.attach||!api.detach)return 0;
 int config=0;uint8_t state=0;void *p=0;*owned=0;
 *open_rc=api.open("/dev/ion",&config,&p);
 if(*open_rc==-1007&&!p){
  if(api.attach("/dev/ion",module)||!*module)return 0;
  *open_rc=api.open("/dev/ion",&config,&p);
 }
 if((*open_rc!=0&&*open_rc!=-1004)||!p)return 0;
 *device=p;
 if(api.state(p,&state))return 0;
 if(*open_rc==-1004)return state==2;
 if(state!=1||api.start(p,&config))return 0;
 *owned=1;
 return !api.state(p,&state)&&state==2;
}
static inline int rs_pool_close(RsPoolApi api,void **device,void **module,int *owned){
 if(!device||!module||!owned||!api.stop||!api.close||!api.detach)return 0;
 if(*device){
  if(*owned){if(api.stop(*device,0))return 0;*owned=0;}
  if(api.close(*device))return 0;
  *device=0;
 }else if(*owned)return 0;
 if(*module){if(api.detach(*module))return 0;*module=0;}
 return 1;
}
#endif
