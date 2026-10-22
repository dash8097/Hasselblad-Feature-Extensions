#ifndef X2D_NATIVE_STARTUP_GUARD_H
#define X2D_NATIVE_STARTUP_GUARD_H
#ifdef __cplusplus
extern "C" {
#endif
enum { X2D_STARTUP_ALLOW=0, X2D_STARTUP_DISABLED=1, X2D_STARTUP_CHECK_FAILED=2 };
/* Fixed read-only lstat. Any existing kind, including dangling link, disables.
 * Only ENOENT permits startup. Never creates, removes or follows the flag. */
int x2d_native_startup_guard(void);
#ifdef __cplusplus
}
#endif
#endif
