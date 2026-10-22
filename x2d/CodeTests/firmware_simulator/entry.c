/* 使用固定 4.2.0 Bionic 的真实进程初始化，不用宿主 glibc 代替。 */
extern int main(int, char **);
typedef struct { void (**preinit)(void), (**init)(void), (**fini)(void); } Structors;
extern __attribute__((noreturn)) void __libc_init(void *, void (*)(void),
    int (*)(int, char **, char **), const Structors *);
static int run(int n, char **v, char **e) { (void)e; return main(n, v); }
__attribute__((noreturn, used)) void simulator_entry(void *stack) {
    const Structors arrays = {0};
    __libc_init(stack, 0, run, &arrays);
}
__attribute__((naked, noreturn)) void _start(void) {
    __asm__ volatile("mov x0, sp\nbl simulator_entry");
}
