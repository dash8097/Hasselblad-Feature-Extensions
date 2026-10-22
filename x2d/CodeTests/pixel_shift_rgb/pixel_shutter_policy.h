#ifndef PIXEL_SHUTTER_POLICY_H
#define PIXEL_SHUTTER_POLICY_H
/* 原厂 4.2.0 Event：仅处理 converted key=0x45；原厂半按 0x41 完整透传。 */
typedef struct { unsigned held, suppressed; } PoShutterPolicy;
/* 返回 0 原厂透传、1 消费重复/释放、2 就绪快门、3 提前快门；
 * 是否保留待拍请求由绑定当前模式应用的后端决定，按住不产生额外请求。 */
static int po_shutter_event(PoShutterPolicy *p,unsigned key,unsigned down,int armed,int ready){
 if(key!=0x45)return 0;
 if(!down){int result=p->held&&p->suppressed;p->held=p->suppressed=0;return result;}
 if(p->held)return p->suppressed?1:0;
 p->held=1;p->suppressed=armed!=0;
 return !armed?0:ready?2:3;
}
#endif
