#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include "pixel_shutter_policy.h"
int main(void){
 PoShutterPolicy p={0};
 for(unsigned k=0;k<256;k++)if(k!=0x45){assert(!po_shutter_event(&p,k,1,1,1));assert(!po_shutter_event(&p,k,0,1,1));}
 assert(!po_shutter_event(&p,0x45,1,0,1));
 assert(!po_shutter_event(&p,0x45,1,1,1)); /* 已透传的按住不能变成新拍摄。 */
 assert(!po_shutter_event(&p,0x45,0,1,1));
 assert(po_shutter_event(&p,0x45,1,1,1)==2);
 assert(po_shutter_event(&p,0x45,1,1,1)==1);
 assert(po_shutter_event(&p,0x45,1,0,1)==1);
 assert(po_shutter_event(&p,0x45,0,0,1)==1); /* 模式退出仍消费对应释放。 */
 assert(po_shutter_event(&p,0x45,1,1,0)==3);
 assert(po_shutter_event(&p,0x45,1,1,1)==1); /* 忙时按住不能在恢复后补拍。 */
 assert(po_shutter_event(&p,0x45,0,1,1)==1);
 assert(po_shutter_event(&p,0x45,1,1,1)==2);
 assert(po_shutter_event(&p,0x45,0,1,1)==1);
 puts("PHYSICAL_SHUTTER_HALF_FORWARD_REPEAT_BUSY_RELEASE_CHECKED");return 0;
}
