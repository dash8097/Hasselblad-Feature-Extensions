/* First-generation X2D 100C 4.2.0, one native hotshoe pulse in a separate process.
 * Offline candidate. It is deliberately inert unless launched as /system/bin/true
 * with X2D_HOTSHOE_FIRE_ONCE=4.2.0. It does not call DjiRCam_Main or open a camera
 * client session. The fixed offsets are verified against the full ELF SHA-256.
 */
#include <stddef.h>
#include <stdint.h>

extern char *getenv(const char *);
extern int open(const char *, int, ...);
extern long read(int, void *, unsigned long);
extern long lseek(int, long, int);
extern int close(int);
extern void *mmap(void *, unsigned long, int, int, int, long);
extern int munmap(void *, unsigned long);
extern long write(int, const void *, unsigned long);
extern int fork(void);
extern int getpid(void);
extern int getppid(void);
extern int waitpid(int, int *, int);
extern void _exit(int) __attribute__((noreturn));
extern void *dlopen(const char *, int);
extern void *dlsym(void *, const char *);
extern int dlclose(void *);
extern unsigned char *SHA256(const unsigned char *, unsigned long, unsigned char *);

static int equal(const char *a, const char *b) {
    if (!a || !b) return 0;
    while (*a && *a == *b) { ++a; ++b; }
    return *a == *b;
}

static int same_bytes(const unsigned char *a, const unsigned char *b, size_t n) {
    for (size_t i = 0; i < n; ++i) if (a[i] != b[i]) return 0;
    return 1;
}

static void line(const char *s) {
    size_t n = 0;
    while (s[n]) ++n;
    (void)write(1, s, n);
}

static int selected_process(void) {
    char arg0[64];
    int fd = open("/proc/self/cmdline", 0);
    if (fd < 0) return 0;
    long n = read(fd, arg0, sizeof(arg0));
    close(fd);
    if (n < 17 || n == (long)sizeof(arg0) || arg0[n - 1] != 0) return 0;
    return equal(arg0, "/system/bin/true");
}

static int verified_binary(void) {
    static const unsigned char expected[32] = {
        0x72,0xeb,0xc8,0xde,0xeb,0xce,0x4a,0x29,
        0x04,0x7c,0x47,0x5e,0x77,0xab,0x4e,0xdb,
        0xf2,0x86,0x0a,0xbb,0x1f,0x57,0x2f,0xea,
        0x45,0x26,0x0f,0xe1,0x7a,0xd0,0xbd,0xa5
    };
    unsigned char actual[32];
    int fd = open("/system/lib64/librcam.so", 0);
    if (fd < 0) return 0;
    long length = lseek(fd, 0, 2);
    if (length < 0x100000 || length > 0x800000) { close(fd); return 0; }
    void *p = mmap(0, (unsigned long)length, 1, 2, fd, 0);
    close(fd);
    if (p == (void *)-1) return 0;
    int ok = SHA256((const unsigned char *)p, (unsigned long)length, actual) != 0
             && same_bytes(actual, expected, sizeof(expected));
    munmap(p, (unsigned long)length);
    return ok;
}

typedef struct { uintptr_t mapped, physical, length; } NativeMap;
typedef int (*init_fn)(void);
typedef void (*deinit_fn)(void);
typedef volatile uint32_t *(*addr_fn)(unsigned, uintptr_t);
typedef void (*set_fn)(int);
typedef void (*sleep_fn)(unsigned);
typedef unsigned (*product_fn)(void);

static int mapped_ok(const NativeMap *m) {
    static const uintptr_t physical[3] = {0xf3000000,0xf3e45000,0xf8154000};
    static const uintptr_t length[3] = {0xc00000,0x1000,0x1000};
    for (unsigned i = 0; i < 3; ++i) {
        if (m[i].physical != physical[i] || m[i].length != length[i] ||
            m[i].mapped == 0 || m[i].mapped == UINTPTR_MAX) return 0;
    }
    return 1;
}

static int fire_once(uintptr_t bias, product_fn product) {
    if (product() != 4) { line("PRODUCT_MISMATCH_NO_FIRE\n"); return 31; }
    NativeMap *maps = (NativeMap *)(bias + 0x2fa878);
    init_fn init = (init_fn)(bias + 0x1afc10);
    deinit_fn deinit = (deinit_fn)(bias + 0x1afde8);
    addr_fn addr = (addr_fn)(bias + 0x1afe78);
    set_fn set = (set_fn)(bias + 0xe2be0);
    sleep_fn sleep_us = (sleep_fn)(bias + 0x1b08e8);
    int status = init();
    if (status != 0 || !mapped_ok(maps)) {
        deinit();
        line("MAP_INIT_FAILED_NO_FIRE\n");
        return 32;
    }
    uintptr_t expected_reg = maps[0].mapped + (0xf330b01c - maps[0].physical);
    volatile uint32_t *reg = addr(0, 0xf330b01c);
    if ((uintptr_t)reg != expected_reg ||
        expected_reg < maps[0].mapped ||
        expected_reg + sizeof(uint32_t) > maps[0].mapped + maps[0].length) {
        deinit();
        line("ADDRESS_MISMATCH_NO_FIRE\n");
        return 33;
    }
    uint32_t before = *reg;
    /* The original routine clears bit 31 on both calls; require it already
     * low, as well as the sync output low, so this test does not alter either
     * bit's starting level after the pulse. Other bits are native RMW.
     */
    if (before & 0x80000001u) {
        deinit();
        line("SYNC_NOT_IDLE_NO_FIRE\n");
        return 34;
    }
    /* A separate process inherits the shared physical mapping. A shared
     * completion word lets it exit if the normal 1 ms pulse ends before its
     * first poll. If the parent dies or stalls with the line high, it lowers
     * it after a bounded wait. Refuse to fire without this recovery process.
     */
    volatile uint32_t *done = mmap(0, 4096, 3, 0x21, -1, 0);
    if (done == (void *)-1) {
        deinit();
        line("WATCHDOG_MAP_FAILED_NO_FIRE\n");
        return 36;
    }
    *done = 0;
    int owner = getpid();
    int watchdog = fork();
    if (watchdog < 0) {
        munmap((void *)done, 4096);
        deinit();
        line("WATCHDOG_START_FAILED_NO_FIRE\n");
        return 36;
    }
    if (watchdog == 0) {
        while (__atomic_load_n(done, __ATOMIC_ACQUIRE) == 0) {
            if (*reg & 0x80000001u) {
                sleep_us(50000);
                if (*reg & 0x80000001u) set(0);
                break;
            }
            if (getppid() != owner) break;
            sleep_us(100);
        }
        munmap((void *)done, 4096);
        deinit();
        _exit(0);
    }
    set(1);
    sleep_us(1000);
    set(0);
    uint32_t after = *reg;
    if (after & 0x80000001u) {
        set(0);
        after = *reg;
    }
    if (!(after & 0x80000001u)) __atomic_store_n(done, 1, __ATOMIC_RELEASE);
    int watchdog_status = -1;
    int watched = waitpid(watchdog, &watchdog_status, 0) == watchdog;
    after = *reg;
    munmap((void *)done, 4096);
    deinit();
    if (!watched || watchdog_status != 0 || (after & 0x80000001u)) {
        line("SYNC_RESTORE_UNCONFIRMED\n");
        return 35;
    }
    line("ONE_NATIVE_PULSE_REQUESTED_AND_LOW\n");
    return 0;
}

static int restore_low(uintptr_t bias, product_fn product) {
    if (product() != 4) { line("PRODUCT_MISMATCH_NO_RESTORE\n"); return 31; }
    NativeMap *maps = (NativeMap *)(bias + 0x2fa878);
    init_fn init = (init_fn)(bias + 0x1afc10);
    deinit_fn deinit = (deinit_fn)(bias + 0x1afde8);
    addr_fn addr = (addr_fn)(bias + 0x1afe78);
    set_fn set = (set_fn)(bias + 0xe2be0);
    int status = init();
    if (status != 0 || !mapped_ok(maps)) {
        deinit();
        line("MAP_INIT_FAILED_NO_RESTORE\n");
        return 32;
    }
    uintptr_t expected_reg = maps[0].mapped + (0xf330b01c - maps[0].physical);
    volatile uint32_t *reg = addr(0, 0xf330b01c);
    if ((uintptr_t)reg != expected_reg ||
        expected_reg < maps[0].mapped ||
        expected_reg + sizeof(uint32_t) > maps[0].mapped + maps[0].length) {
        deinit();
        line("ADDRESS_MISMATCH_NO_RESTORE\n");
        return 33;
    }
    uint32_t before = *reg;
    if (before & 0x80000000u) {
        deinit();
        line("SYNC_CONTROL_NOT_OWNED_NO_RESTORE\n");
        return 37;
    }
    if (before & 1u) set(0);
    int ok = !(*reg & 0x80000001u);
    deinit();
    line(ok ? "SYNC_LOW_CONFIRMED\n" : "SYNC_RESTORE_UNCONFIRMED\n");
    return ok ? 0 : 35;
}

__attribute__((constructor)) static void entry(void) {
    int fire = equal(getenv("X2D_HOTSHOE_FIRE_ONCE"), "4.2.0");
    int inspect = equal(getenv("X2D_HOTSHOE_INSPECT"), "4.2.0");
    int restore = equal(getenv("X2D_HOTSHOE_RESTORE"), "4.2.0");
    if (!fire && !inspect && !restore) return;
    if (fire + inspect + restore != 1) {
        line("AMBIGUOUS_MODE_NO_ACTION\n"); _exit(19);
    }
    if (!selected_process()) { line("WRONG_HOST_NO_FIRE\n"); _exit(20); }
    if (!verified_binary()) { line("LIBRCAM_HASH_MISMATCH_NO_FIRE\n"); _exit(21); }
    void *handle = dlopen("/system/lib64/librcam.so", 2);
    if (!handle) { line("LIBRCAM_LOAD_FAILED_NO_FIRE\n"); _exit(22); }
    product_fn product = (product_fn)dlsym(handle, "producttype_get_product_id");
    if (!product) { dlclose(handle); line("PRODUCT_SYMBOL_MISSING_NO_FIRE\n"); _exit(23); }
    uintptr_t bias = (uintptr_t)product - 0x384f8;
    if (inspect) {
        int ok = product() == 4;
        dlclose(handle);
        line(ok ? "LOADED_PRODUCT4_NO_FIRE\n" : "PRODUCT_MISMATCH_NO_FIRE\n");
        _exit(ok ? 0 : 31);
    }
    int result = fire ? fire_once(bias, product) : restore_low(bias, product);
    dlclose(handle);
    _exit(result);
}
