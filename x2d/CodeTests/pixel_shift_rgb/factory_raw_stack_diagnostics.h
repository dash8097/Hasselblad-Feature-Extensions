/* 官方 X2D 4.2.0：只记录固定诊断类别，不输出 D-Bus 文本或照片元数据。
 * DBusError 布局核对本地 Qt 5.5.1 的 dbus_minimal_p.h；链接到固件 libdbus。
 */
#ifndef FACTORY_RAW_STACK_DIAGNOSTICS_H
#define FACTORY_RAW_STACK_DIAGNOSTICS_H
#include <stddef.h>
typedef struct {
 const char *name,*message;
 unsigned int dummy1:1,dummy2:1,dummy3:1,dummy4:1,dummy5:1;
 void *padding1;
} StackDBusError;
_Static_assert(sizeof(StackDBusError)==32 && offsetof(StackDBusError,padding1)==24,
 "RAW stack requires the verified 64-bit DBusError ABI");
extern void dbus_error_init(void *);
extern void dbus_error_free(void *);
static const char *stack_dbus_error_category(const char *name){
 const char *allowed[]={"org.freedesktop.DBus.Error.NoReply","org.freedesktop.DBus.Error.Timeout",
  "org.freedesktop.DBus.Error.TimedOut","org.freedesktop.DBus.Error.Disconnected",
  "org.freedesktop.DBus.Error.ServiceUnknown","org.freedesktop.DBus.Error.NameHasNoOwner",
  "org.freedesktop.DBus.Error.UnknownMethod","org.freedesktop.DBus.Error.NoMemory",
  "org.freedesktop.DBus.Error.AccessDenied","org.freedesktop.DBus.Error.LimitsExceeded",
  "org.freedesktop.DBus.Error.Failed","com.hasselblad.error.canceled"};
 if(!name)return "ABSENT";
 for(unsigned i=0;i<sizeof allowed/sizeof allowed[0];i++)if(same(name,allowed[i]))return allowed[i];
 return "OTHER_REDACTED";
}
static void stack_diagnostic_status(const char *operation,int status){
 text("RAW_STACK_RENDER_STATUS operation=");text(operation);text(" rc=");
 if(status<0){text("-");number((unsigned)(-(int64_t)status));}else number((unsigned)status);
 text("\n");
}
#endif
