/* 只存在于无外设透传的 guest；不是相机启动脚本。 */
#define _GNU_SOURCE
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/reboot.h>
#include <sys/resource.h>
#include <sys/sysmacros.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

static void off(int ok) {
    puts(ok ? "X2D_SIM_ALL_PASSED" : "X2D_SIM_FAILED");
    sync(); reboot(RB_POWER_OFF); for (;;) pause();
}
static long long now(void) {
    struct timespec t; if (clock_gettime(CLOCK_MONOTONIC, &t)) off(0);
    return (long long)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}
static int child(const char *path, const char *arg, unsigned deadline, int expect) {
    pid_t pid = fork(); if (pid < 0) return 0;
    if (!pid) {
        struct rlimit core = {0,0}; setrlimit(RLIMIT_CORE, &core);
        setenv("LD_LIBRARY_PATH", "/system/lib64", 1);
        execl(path, path, !strcmp(path,"/sim") ? arg : (const char *)0, (char *)0); _exit(127);
    }
    int status = 0, expired = 0; long long start = now();
    for (;;) {
        pid_t got = waitpid(pid, &status, WNOHANG);
        if (got == pid) break;
        if (got < 0 && errno != EINTR) return 0;
        if (now() - start >= deadline) {
            expired = 1; kill(pid, SIGKILL);
            while (waitpid(pid, &status, 0) < 0) if (errno != EINTR) return 0;
            break;
        }
        struct timespec delay = {0,10000000}; nanosleep(&delay, 0);
    }
    int rc = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
    printf("X2D_SIM_CASE name=%s rc=%d timeout=%d expected=%d\n", arg, rc, expired, expect);
    return expect == -1 ? expired && rc == 137 : !expired && rc == expect;
}
int main(void) {
    if (getpid() != 1) return 2;
    if (mount("devtmpfs", "/dev", "devtmpfs", 0, 0)) return 3;
    int fd = open("/dev/console", O_RDWR);
    if (fd < 0) { mknod("/dev/console", S_IFCHR|0600, makedev(5,1)); fd = open("/dev/console", O_RDWR); }
    if (fd < 0) return 4;
    dup2(fd,0); dup2(fd,1); dup2(fd,2); if (fd > 2) close(fd);
    setbuf(stdout,0); setbuf(stderr,0);
    if (mount("proc", "/proc", "proc", 0, 0) || mount("tmpfs", "/tmp", "tmpfs", 0, "size=32m")) off(0);
    puts("X2D_SIM_BEGIN firmware=4.2.0 guest_kernel=generic_linux hardware=none");
    /* 全新虚拟机内的故障用例，确认退出/崩溃/超时不会被当成通过。 */
    if (!child("/sim", "runtime", 20000, 0)) off(0);
    if (!child("/sim", "400mp", 120000, 0)) off(0);
    if (!child("/sixteen-edge", "1600mp-source-edge", 30000, 0)) off(0);
    if (!child("/quarter-pixels", "25mp-pixels", 120000, 0)) off(0);
    if (!child("/sim", "stack", 120000, 0)) off(0);
    if (!child("/sim", "unknown", 5000, 64)) off(0);
    if (!child("/sim", "expected-failure", 5000, 19)) off(0);
    if (!child("/sim", "expected-crash", 5000, 139)) off(0);
    if (!child("/sim", "expected-timeout", 300, -1)) off(0);
    /* 验证真实固件依赖缺失会失败，禁止悄悄换成宿主库。 */
    if (rename("/system/lib64/libc.so", "/system/lib64/libc.hidden")) off(0);
    if (!child("/sim", "missing-runtime", 5000, 1)) off(0);
    if (rename("/system/lib64/libc.hidden", "/system/lib64/libc.so")) off(0);
    if (!child("/sim", "runtime", 20000, 0)) off(0);
    off(1);
}
