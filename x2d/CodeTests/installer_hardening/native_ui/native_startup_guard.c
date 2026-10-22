#define _GNU_SOURCE
#include "native_startup_guard.h"
#include <sys/stat.h>
#include <errno.h>
int x2d_native_startup_guard(void){
 struct stat value;
 if(lstat("/blackbox/x2d-native-menu.disable",&value)==0)return X2D_STARTUP_DISABLED;
 return errno==ENOENT?X2D_STARTUP_ALLOW:X2D_STARTUP_CHECK_FAILED;
}
