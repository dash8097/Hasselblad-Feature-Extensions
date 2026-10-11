# 像素处理组件

`prototype.py` 提供四帧合成的人工场景模型；`capture_memory_merge.c`、`capture_memory_pipeline.h` 和 `pixel_chunk_fanout.h` 负责六帧逐行重建及双消费者生命周期。RAW 分块、缩放与轻度亮度锐化使用调用方提供的内存或回调。网格容量与按键编号由调用方显式传入。文件名中的历史前缀不表示厂商实现。
