/* 十六张候选独立参数；不能覆盖六张或普通堆栈的编码策略。 */
#ifndef SIXTEEN_DISK_POLICY_H
#define SIXTEEN_DISK_POLICY_H
#include <stdint.h>
#define X2D_SIXTEEN_FRAMES 16u
#define X2D_SIXTEEN_SOURCE_ROWS 1024u
#define X2D_SIXTEEN_JPEG_QUALITY 60u
#define X2D_SIXTEEN_JPEG_CAPACITY UINT32_C(536870912)
#define X2D_SIXTEEN_OUTPUT_WIDTH 46652u
#define X2D_SIXTEEN_OUTPUT_HEIGHT 34996u
/* 官方 4.2.0 camera-service，setMultiShotPosition mode=3，表 0x5f1918。
 * 单位为半像素。完整 RGBG 半像素网格再采样到四倍宽高 CFA。
 * 十六亿输出不是原厂四分之一像素轨迹，不宣称独立解析力。 */
static const uint8_t x2d_sixteen_phase[16][2]={
 {0,0},{0,1},{0,2},{0,3},{1,3},{1,2},{1,1},{1,0},
 {2,0},{2,1},{2,2},{2,3},{3,3},{3,2},{3,1},{3,0}
};
#endif
