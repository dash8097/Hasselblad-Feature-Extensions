#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct X2dPassiveOutput X2dPassiveOutput;
enum {X2D_PASSIVE_PLAYBACK=0,X2D_PASSIVE_UI=1};
X2dPassiveOutput*x2d_passive_create(void);
/* GUI single producer. Copies only; no filesystem or wait. Replaces a queued
 * snapshot of the same kind; returns 0 if the worker currently owns that slot.
 * Accepted does not mean written; diagnostics never imply operation success. */
int x2d_passive_publish(X2dPassiveOutput*,int kind,const char*,size_t,uint64_t deadline_ms);
void x2d_passive_stop(X2dPassiveOutput*);
int x2d_passive_release(X2dPassiveOutput*);
uint64_t x2d_passive_written(const X2dPassiveOutput*,int kind);
uint64_t x2d_passive_processed(const X2dPassiveOutput*,int kind);
int x2d_passive_error(const X2dPassiveOutput*,int kind);
#ifdef __cplusplus
}
#endif
