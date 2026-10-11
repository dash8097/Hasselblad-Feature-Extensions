/* Windows 验证 DLL 的无状态入口；不加载其他组件。 */
unsigned long _tls_index;
int DllMain(void *module, unsigned long reason, void *reserved)
{ (void)module; (void)reason; (void)reserved; return 1; }
