/* 测试断言的 GNU 头文件 -> Bionic 适配；不替换生产函数。 */
#include <stdio.h>
#include <stdlib.h>
__attribute__((noreturn)) void sim_assert_fail(const char *expr, const char *file,
                                              unsigned line, const char *function) {
    printf("X2D_SIM_ASSERT %s:%u %s %s\n", file, line, function, expr);
    exit(70);
}
