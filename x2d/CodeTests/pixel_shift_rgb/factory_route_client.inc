#include "pixel_render_route.h"
#define FACTORY_ROUTE_PATH "/tmp/x2d-pixel-overclock/render-lease"
#define FACTORY_ROUTE_TEMP "/tmp/x2d-pixel-overclock/render-lease.tmp"
static int factory_route_owned;
static uint64_t factory_route_ticks;
static PoRenderLease factory_route_latest;
static int factory_native_group(void *bus){
 const char *iface="com.hasselblad.camera",*key="stream_group";
 void *m=dbus_message_new_method_call(iface,"/camera","org.freedesktop.DBus.Properties","Get");
 if(!m)return -1;
 int built=dbus_message_append_args(m,'s',&iface,'s',&key,0);
 void *r=built?dbus_connection_send_with_reply_and_block(bus,m,3000,0):0;dbus_message_unref(m);
 int32_t value=-1;Iter it={0},inner={0};
 if(r&&dbus_message_get_type(r)==2&&dbus_message_iter_init(r,&it)&&dbus_message_iter_get_arg_type(&it)=='v'){
  dbus_message_iter_recurse(&it,&inner);
  if(dbus_message_iter_get_arg_type(&inner)=='i')dbus_message_iter_get_basic(&inner,&value);
 }
 if(r)dbus_message_unref(r);return value>=0&&value<=255?value:-1;
}
static int factory_wait_native_idle(void *bus,unsigned retries){
 for(unsigned i=0;i<retries;i++){
  int group=factory_native_group(bus);if(group==255)return 1;if(group<0)return 0;
  long delay[2]={0,50000000};if(sc(101,(long)delay,0,0,0))return 0;
 }
 return 0;
}
static int factory_stop_native(void *bus){
 /* 原请求完成后的状态发布是异步的，先等待自然退出。 */
 if(factory_wait_native_idle(bus,20))return 1;
 void *m=dbus_message_new_method_call("com.hasselblad.camera","/camera","com.hasselblad.camera","reprocess_stop_session");
 if(!m)return 0;
 void *r=dbus_connection_send_with_reply_and_block(bus,m,20000,0);dbus_message_unref(m);
 int ok=r&&dbus_message_get_type(r)==2;
 if(ok){Iter it={0};ok=!dbus_message_iter_init(r,&it);}
 if(r&&dbus_message_get_type(r)==3){
  const char *error=dbus_message_get_error_name(r);
  if(error&&same(error,"com.hasselblad.error.canceled"))ok=1;
 }
 if(r)dbus_message_unref(r);
 return ok&&factory_wait_native_idle(bus,200);
}
static int factory_route_publish(const uint64_t *identities){
 if(!identities[0]||!identities[1]||identities[0]==identities[1])return 0;
 uint32_t pid=(uint32_t)sc(172,0,0,0,0);
 if(!factory_route_ticks){
  char stat[2048];int f=(int)sc(56,-100,(long)"/proc/self/stat",0xA0000,0);
  if(f<0)return 0;long n=sc(63,f,(long)stat,sizeof stat-1,0);sc(57,f,0,0,0);
  if(n<=0||n==(long)sizeof stat-1||!po_render_start_ticks(stat,(size_t)n,&factory_route_ticks))return 0;
 }
 int f=(int)sc(56,-100,(long)FACTORY_ROUTE_PATH,0xA0000,0);
 if(f>=0){
  PoRenderLease old;long n=sc(63,f,(long)&old,sizeof old,0);sc(57,f,0,0,0);
  if(!factory_route_owned||n!=sizeof old||old.magic!=PO_RENDER_MAGIC||old.pid!=pid||old.start_ticks!=factory_route_ticks)return 0;
 }else if(f!=-2||factory_route_owned)return 0;
 PoRenderLease lease={PO_RENDER_MAGIC,pid,0,factory_route_ticks,jpeg_monotonic_ms()+PO_RENDER_LEASE_MS,identities[0],identities[1]};
 f=(int)sc(56,-100,(long)FACTORY_ROUTE_TEMP,0xA00C1,0600);if(f<0)return 0;
 int ok=sc(64,f,(long)&lease,sizeof lease,0)==sizeof lease&&sc(82,f,0,0,0)==0;
 if(sc(57,f,0,0,0))ok=0;
 if(ok&&!sc(38,-100,(long)FACTORY_ROUTE_TEMP,-100,(long)FACTORY_ROUTE_PATH)){factory_route_latest=lease;factory_route_owned=1;return 1;}
 sc(35,-100,(long)FACTORY_ROUTE_TEMP,0,0);return 0;
}
static int factory_route_acknowledged(void){
 int fd=(int)sc(56,-100,(long)"/tmp/x2d-pixel-overclock/render-accepted",0xA0000,0);if(fd<0)return 0;
 PoRenderLease ack;long n=sc(63,fd,(long)&ack,sizeof ack,0);sc(57,fd,0,0,0);
 if(n!=sizeof ack)return 0;
 const unsigned char *a=(const unsigned char*)&ack,*b=(const unsigned char*)&factory_route_latest;
 for(unsigned i=0;i<sizeof ack;i++)if(a[i]!=b[i])return 0;
 return 1;
}
static int factory_route_clear(void){
 if(!factory_route_owned)return 1;
 int fd=(int)sc(56,-100,(long)FACTORY_ROUTE_PATH,0xA0000,0);if(fd<0)return 0;
 PoRenderLease old;long n=sc(63,fd,(long)&old,sizeof old,0);sc(57,fd,0,0,0);
 if(n!=sizeof old||old.magic!=PO_RENDER_MAGIC||old.pid!=(uint32_t)sc(172,0,0,0,0)||old.start_ticks!=factory_route_ticks)return 0;
 if(sc(35,-100,(long)FACTORY_ROUTE_PATH,0,0))return 0;
 fd=(int)sc(56,-100,(long)"/tmp/x2d-pixel-overclock/render-accepted",0xA0000,0);
 if(fd>=0){
  PoRenderLease ack;n=sc(63,fd,(long)&ack,sizeof ack,0);sc(57,fd,0,0,0);
  if(n!=sizeof ack||ack.magic!=PO_RENDER_MAGIC||ack.pid!=old.pid||ack.start_ticks!=old.start_ticks)return 0;
  if(sc(35,-100,(long)"/tmp/x2d-pixel-overclock/render-accepted",0,0))return 0;
 }else if(fd!=-2)return 0;
 factory_route_owned=0;return 1;
}
