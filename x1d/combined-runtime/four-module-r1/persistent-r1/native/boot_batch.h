#pragma once
#include "boot_loader.h"
// 离线候选接口；现有 UART 不实现此协议，稳定入口仍使用原有串行 I/O。
struct HblBootCompare { uint32_t address,value,mask; };
struct HblBootBatchIO : HblBootIO {
    // 微型接收器阶段：只向已绑定固定暂存区顺序传输，完成后才开启正式批量协议。
    virtual bool uploadSeed(uint32_t,const uint32_t *,size_t) {return false;}
    virtual bool earlyBootstrapAllowed() const {return false;}
    virtual bool needsAdapter() const {return false;}
    virtual bool batchesReady() const {return true;}
    virtual uint32_t batchSession() const {return 0;}
    virtual bool activateBatches(bool) {return false;}

    // 整批严格有序执行，首个失败立即停止；不重发，不跨阶段合并。
    // 接收端必须先验证会话、序号、完整消息及全部地址/值授权，再执行。
    // 成功仅表示全部逐字精确比较通过，不能使用抽样或摘要代替。
    virtual bool compareBatch(const HblBootCompare *,size_t)=0;
    // 每字写入后立即读回比较；批内失败可能已经部分写入，禁止重试。
    virtual bool uploadBatch(uint32_t,const uint32_t *,size_t)=0;
};
bool hbl_load_boot_modules_batch(HblBootBatchIO &);

// UI+AF timing profile: no flash payload or flash hooks are published.
bool hbl_load_boot_af_batch(HblBootBatchIO &io);
