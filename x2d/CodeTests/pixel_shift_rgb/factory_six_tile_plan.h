/* 六次显影覆盖：四块主体，整条右边，整条底边；保留原厂滤波周边。 */
#ifndef FACTORY_SIX_TILE_PLAN_H
#define FACTORY_SIX_TILE_PLAN_H
#include "factory_buffer_geometry.h"
typedef struct {uint32_t x,y,width,height;FactoryBufferGeometry raw,yuv;} FactorySixTile;
static int factory_six_tile(unsigned index,FactorySixTile *out){
 if(!out||index>=6)return 0;FactorySixTile t={0};
 if(index<4){t.x=(index%2)*11654;t.y=(index/2)*8742;t.width=11654;t.height=8742;}
 else if(index==4){t.x=23308;t.width=18;t.height=17498;}
 else{t.y=17484;t.width=23308;t.height=14;}
 /* 左侧多两列：原厂裁剪 128 列，六合一采样偏一列，复制时丢弃该列。 */
 if(!factory_buffer_geometry(&t.yuv,t.width+2,t.height,6)||
    !factory_buffer_geometry(&t.raw,t.yuv.width+180,t.yuv.height+100,1))return 0;
 *out=t;return 1;
}
#endif
