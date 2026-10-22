/* 第一代 X2D 4.2.0，内部离线迁移样例；不包含任意命令入口。 */
#ifndef X2D_CAMERA_NATIVE_H
#define X2D_CAMERA_NATIVE_H
#include <stddef.h>
#include <stdint.h>
enum cn_phase { CN_ARGS, CN_VALIDATE, CN_MEMORY, CN_ENCODE, CN_VERIFY, CN_DONE };
struct cn_ops {
    void *ctx;
    int (*prepare)(void *, const char *);
    int (*memory)(void *, uint64_t *);
    int (*delay)(void *);
    int (*cancelled)(void *);
    int (*encode)(void *, const char *);
    int (*verify)(void *);
    int (*audio)(void *);
    void (*event)(void *, const char *);
};
struct cn_result { int rc; enum cn_phase phase; unsigned memory_reads; };
int cn_job_valid(const char *);
int cn_mem_available(const char *, size_t, uint64_t *);
struct cn_result cn_run(int, const char *const *, const struct cn_ops *);
#endif
