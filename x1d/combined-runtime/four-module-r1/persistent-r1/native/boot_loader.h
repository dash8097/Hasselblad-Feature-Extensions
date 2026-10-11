#pragma once
#include <stdint.h>
#include <stddef.h>
// I/O 的失败必须是终止状态；不会要求重发写入、SGI 或重新分配。
struct HblBootIO {
    virtual ~HblBootIO() {}
    virtual bool version()=0;
    virtual bool read(uint32_t,uint32_t *)=0;
    virtual bool write(uint32_t,uint32_t)=0;
    virtual bool checkpoint(const char *,uint32_t,uint32_t,bool)=0;
    virtual bool pause(unsigned)=0;
};
bool hbl_load_boot_modules(HblBootIO &);
bool hbl_relocate_af(uint32_t base,uint32_t *output,size_t words);
