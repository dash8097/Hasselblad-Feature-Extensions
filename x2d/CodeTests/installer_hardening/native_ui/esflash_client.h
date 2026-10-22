#ifndef X2D_ESFLASH_CLIENT_H
#define X2D_ESFLASH_CLIENT_H
#include "esflash_protocol.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {int fd,ready,pending,connecting;unsigned pending_operation,queued,queued_profile,queued_delay;uint64_t next,deadline;X2dFlashReply state;} X2dFlashClient;
void x2d_flash_client_init(X2dFlashClient*);
void x2d_flash_client_close(X2dFlashClient*);
void x2d_flash_client_poll(X2dFlashClient*,uint64_t);
int x2d_flash_client_set(X2dFlashClient*,unsigned,unsigned,uint64_t);
#ifdef __cplusplus
}
#endif
#endif
