/* Optional preload bootstrap. It only registers with the exact original GUI;
 * the registration entry rejects every other executable before calling Qt. */
extern int x2d_native_ui_register_v1(void);
__attribute__((constructor))static void x2d_native_bootstrap(void){(void)x2d_native_ui_register_v1();}
