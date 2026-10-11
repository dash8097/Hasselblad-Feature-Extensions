/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hasselblad Feature Extensions contributors
 */
#ifndef PIXEL_SHUTTER_POLICY_H
#define PIXEL_SHUTTER_POLICY_H
/* 边沿与重复按键策略；调用方提供关注的按键编号。 */
typedef struct { unsigned held, suppressed; } PoShutterPolicy;
/* 返回 0 原厂透传、1 消费不触发、2 消费且请求一次；不保存忙时拍摄。 */
static int po_shutter_event(PoShutterPolicy *p,unsigned key,unsigned down,int armed,int ready,unsigned trigger_key){
 if(key!=trigger_key)return 0;
 if(!down){int result=p->held&&p->suppressed;p->held=p->suppressed=0;return result;}
 if(p->held)return p->suppressed?1:0;
 p->held=1;p->suppressed=armed!=0;
 return !armed?0:ready?2:1;
}
#endif
