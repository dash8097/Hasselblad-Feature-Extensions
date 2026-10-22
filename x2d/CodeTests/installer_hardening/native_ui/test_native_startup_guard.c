#define _GNU_SOURCE
#include "native_startup_guard.h"
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/reboot.h>
static unsigned checks;
#define CK(x) do{++checks;if(!(x)){printf("STARTUP_GUARD_FAIL line=%d\n",__LINE__);reboot(RB_POWER_OFF);_exit(1);}}while(0)
static const char*flag="/blackbox/x2d-native-menu.disable";
int main(void){setvbuf(stdout,NULL,_IONBF,0);
 CK(x2d_native_startup_guard()==X2D_STARTUP_ALLOW);CK(!mkdir("/blackbox",0700));CK(x2d_native_startup_guard()==X2D_STARTUP_ALLOW);
 int fd=open(flag,O_CREAT|O_WRONLY,0000);CK(fd>=0);CK(!close(fd));CK(x2d_native_startup_guard()==X2D_STARTUP_DISABLED);CK(!unlink(flag));
 CK(!symlink("/not-present",flag));CK(x2d_native_startup_guard()==X2D_STARTUP_DISABLED);CK(access("/not-present",F_OK));CK(!unlink(flag));
 CK(!mkfifo(flag,0600));CK(x2d_native_startup_guard()==X2D_STARTUP_DISABLED);CK(!unlink(flag));
 CK(!mkdir(flag,0700));CK(x2d_native_startup_guard()==X2D_STARTUP_DISABLED);CK(!rmdir(flag));
 pid_t child=fork();CK(child>=0);if(child==0){if(setuid(1001))_exit(2);_exit(x2d_native_startup_guard()==X2D_STARTUP_CHECK_FAILED?0:3);}int status;CK(waitpid(child,&status,0)==child);CK(WIFEXITED(status)&&WEXITSTATUS(status)==0);
 CK(!rmdir("/blackbox"));fd=open("/blackbox",O_CREAT|O_WRONLY,0600);CK(fd>=0);CK(!close(fd));CK(x2d_native_startup_guard()==X2D_STARTUP_CHECK_FAILED);CK(!unlink("/blackbox"));
 CK(!symlink("/blackbox","/blackbox"));CK(x2d_native_startup_guard()==X2D_STARTUP_CHECK_FAILED);CK(!unlink("/blackbox"));CK(x2d_native_startup_guard()==X2D_STARTUP_ALLOW);
 printf("STARTUP_GUARD_PASS assertions=%u actual_ARM_Linux=1 camera_operations=0\n",checks);reboot(RB_POWER_OFF);return 0;
}
