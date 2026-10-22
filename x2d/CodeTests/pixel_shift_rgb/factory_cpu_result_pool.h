/* 4.2.0：整幅 Bayer 结果仅供 CPU 合成、写 RAW 和填充 HISO 输入。
 * 使用可共享的 heap2，避免占用原厂六帧和取景所用的 2 GiB heap23。
 * 真正送显影器/编码器的 RAW、YUV 和 JPEG 缓冲继续使用 heap23。 */
#ifndef FACTORY_CPU_RESULT_POOL_H
#define FACTORY_CPU_RESULT_POOL_H
#define FACTORY_CPU_RESULT_HEAP 2u
#endif
