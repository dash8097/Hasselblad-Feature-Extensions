/* X2D 100C 4.2.0：编码前NV16缩小。无构造函数、无设备连接、无挂接。 */
#ifndef QUARTER_DIRECT_JPEG_H
#define QUARTER_DIRECT_JPEG_H
#include <stddef.h>
#include <stdint.h>
enum { QD_WIDTH=5828, QD_HEIGHT=4371, QD_STORED_HEIGHT=4372,
       QD_STRIDE=5888, QD_BYTES=QD_STRIDE*QD_STORED_HEIGHT*2, QD_QUALITY=80 };
/* 输入/输出内存的持有由调用方负责，缩小函数不释放任何原厂缓冲。
 * 必须先按本次拍摄身份冻结模式；禁止用编码时的全局最新模式代替。
 * 本接口不推断frame编号与ExposureId等价。 */
typedef struct {
 const uint8_t *pixels;
 size_t bytes;
 uint32_t width,height,stride;
} QdNv16;
/* 返回1成功，0拒绝；只接受正式一亿NV16。输出末行复制用于偶数行编码。
 * 调用方完成CPU/DEVICE同步后，才可提交quality=80的HAL编码请求。
 * 编码成功完成后才归还私有缓冲；错误返回须先确认DMA终止。 */
int qd_resize(const QdNv16 *source,uint8_t *output,size_t capacity);
/* 构造私有HAL描述符，不修改借用的原厂frame/stream。
 * 成片header中的4372->4371由存储端qj_quarter_visible处理，不重编码。 */
int qd_descriptors(uint8_t frame[0x218],uint8_t stream[0x1c8],
 void *pixels_handle,void *jpeg_handle,uint32_t jpeg_capacity);
#endif
