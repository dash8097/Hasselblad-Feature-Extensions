#ifndef X2D_LOADED_MODULE_IDENTITY_H
#define X2D_LOADED_MODULE_IDENTITY_H
#ifdef __cplusplus
extern "C" {
#endif
/* Worker thread only: exact fixed file, executable mapping device/inode and
 * stable full-file SHA. This is not attestation against in-process patching. */
int x2d_loaded_module_hash(char out[65]);
#ifdef __cplusplus
}
#endif
#endif
