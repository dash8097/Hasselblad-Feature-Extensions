#include <stdio.h>
#include <stdlib.h>
typedef __UINTPTR_TYPE__ U;
typedef __INT64_TYPE__ I64;
#define FACE_BACKEND_TEST 1
#include "eye_af_bridge.h"
static int checks,queues,handle=2,actual=1,pending=-1,valid=0,queue_rc=0;
#define C(x) do{++checks;if(!(x)){fprintf(stderr,"failure line %d\n",__LINE__);exit(1);}}while(0)
static int get_handle(void){return handle;}
static int get_valid(int h){C(h==handle);return valid;}
static int get_eye(int h){C(h==handle);return actual;}
static int queue(int h,const unsigned*c){C(h==handle&&c[0]==0x203&&c[1]==1&&c[2]==16&&c[3]<=3);++queues;pending=(int)c[3];return queue_rc;}
int main(void){
 eye_handle=get_handle;eye_valid_handle=get_valid;eye_get=get_eye;eye_queue=queue;
 eye_test_now=100;C(eye_command(EYE_COMMAND+2)==1);C(queues==1&&actual==1&&pending==2&&eye_test_report[1]==2&&eye_test_report[2]==1&&eye_test_times[0]==100);
 actual=pending;eye_test_now=600;C(eye_command(EYE_OBSERVE)==1);C(queues==1&&eye_test_report[2]==2&&eye_test_report[3]==0&&eye_test_times[0]==100&&eye_test_times[1]==600);
 for(int n=0;n<100;n++){eye_test_now++;C(eye_command(EYE_OBSERVE)==1&&queues==1);}
 handle=3;C(eye_command(EYE_OBSERVE)==1&&queues==1&&eye_test_report[2]==-1&&eye_test_report[3]==-1);handle=2;
 valid=1;C(eye_command(EYE_OBSERVE)==1&&queues==1&&eye_test_report[2]==-1);valid=0;
 actual=4;C(eye_command(EYE_OBSERVE)==1&&queues==1&&eye_test_report[2]==-1);actual=2;
 eye_test_now=900;C(eye_command(EYE_COMMAND+4)==1&&queues==2&&pending==1&&eye_test_report[0]==4&&eye_test_report[1]==1);actual=pending;
 eye_test_now=1000;C(eye_command(EYE_OBSERVE)==1&&queues==2&&eye_test_report[2]==1&&eye_test_times[0]==900);
 queue_rc=-2;eye_test_now=1100;C(eye_command(EYE_COMMAND+3)==1&&queues==3);actual=3;C(eye_command(EYE_OBSERVE)==1&&eye_test_report[3]==-2&&queues==3);
 eye_handle=0;C(eye_command(EYE_OBSERVE)==1&&eye_test_report[3]==-1&&queues==3);C(eye_command(EYE_COMMAND+6)==0);C(eye_command(EYE_COMMAND-1)==0);
 /* 系统时钟失效不能生成新鲜确认。 */
 eye_test_now=-1;C(eye_command(EYE_COMMAND)==1&&eye_test_times[0]==-1);C(eye_command(EYE_OBSERVE)==1&&eye_test_times[1]==-1&&queues==3);
 printf("EYE_READONLY_BACKEND_PASS assertions=%d readonly_samples=108 setter_queues=%d hardware_calls=0\n",checks,queues);return 0;
}
