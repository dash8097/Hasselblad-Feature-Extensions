/* 官方 4.2.0 一亿 ImageMemory 基线：宽度与填充步长分别核验。
 * 来源为已有 factory-native-descriptor-geometry.json；真实拍摄仍待核对。
 * 文件里的 RAW 宽度 11904 不等于 ImageMemory 的有效宽度 11836。
 */
#ifndef PRIVATE_CAPTURE_RAW_PROFILE_H
#define PRIVATE_CAPTURE_RAW_PROFILE_H
#include <stdint.h>
#include <stddef.h>

#define X2D_CAPTURE_RAW_WIDTH 11836U
#define X2D_CAPTURE_RAW_HEIGHT 8842U
#define X2D_CAPTURE_RAW_ROW_BYTES 23808U
#define X2D_CAPTURE_RAW_CONTENT_BYTES INT64_C(210510336)
enum {
 X2D_CAPTURE_RAW_VALID=0,
 X2D_CAPTURE_RAW_FORMAT_REJECTED,
 X2D_CAPTURE_RAW_WIDTH_REJECTED,
 X2D_CAPTURE_RAW_HEIGHT_REJECTED,
 X2D_CAPTURE_RAW_STRIDE_REJECTED,
 X2D_CAPTURE_RAW_ALLOCATION_REJECTED,
 X2D_CAPTURE_RAW_DATA_REJECTED,
 X2D_CAPTURE_RAW_LENGTH_REJECTED
};
static inline unsigned x2d_capture_raw_geometry(uint32_t width,uint32_t height,
 uint8_t format,uint32_t row_bytes){
 if(format!=1)return X2D_CAPTURE_RAW_FORMAT_REJECTED;
 if(width!=X2D_CAPTURE_RAW_WIDTH)return X2D_CAPTURE_RAW_WIDTH_REJECTED;
 if(height!=X2D_CAPTURE_RAW_HEIGHT)return X2D_CAPTURE_RAW_HEIGHT_REJECTED;
 if(row_bytes!=X2D_CAPTURE_RAW_ROW_BYTES)return X2D_CAPTURE_RAW_STRIDE_REJECTED;
 return X2D_CAPTURE_RAW_VALID;
}
static inline unsigned x2d_capture_raw_view(int owns_allocation,int has_data,intptr_t length){
 if(owns_allocation)return X2D_CAPTURE_RAW_ALLOCATION_REJECTED;
 if(!has_data)return X2D_CAPTURE_RAW_DATA_REJECTED;
 if(length<X2D_CAPTURE_RAW_CONTENT_BYTES)return X2D_CAPTURE_RAW_LENGTH_REJECTED;
 return X2D_CAPTURE_RAW_VALID;
}
#endif
