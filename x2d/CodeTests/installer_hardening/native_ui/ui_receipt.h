#ifndef X2D_UI_RECEIPT_H
#define X2D_UI_RECEIPT_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define X2D_UI_QUERY_PATH "/tmp/x2d-native-ui.query"
#define X2D_UI_RECEIPT_PATH "/tmp/x2d-native-ui.status"
enum X2dUiReceiptState {X2D_UI_ATTACHING=1,X2D_UI_READY=2,X2D_UI_FAILED=3,X2D_UI_STOPPED=4};
typedef struct {char nonce[33];uint64_t generation;} X2dUiQuery;
typedef struct {uint64_t pid,start_ticks,generation,sampled_ms,capabilities;int state;char nonce[33],build[65],gui[65];} X2dUiReceipt;
int x2d_ui_query_parse(const char*,size_t,char nonce[33]);
int x2d_ui_receipt_parse(const char*,size_t,X2dUiReceipt*);
size_t x2d_ui_receipt_format(const X2dUiReceipt*,char*,size_t);
/* Expected nonce is unique to this diagnostic request. An old status file or
 * a fresh-looking timestamp alone is never accepted as readiness evidence. */
int x2d_ui_receipt_ready(const X2dUiReceipt*,const char*nonce,uint64_t pid,uint64_t start_ticks,const char*build,const char*gui,uint64_t required_caps,uint64_t request_ms,uint64_t now_ms);
typedef struct X2dUiReceiptWorker X2dUiReceiptWorker;
/* GUI thread: fixed-size atomics/memory only. All files are accessed by worker.
 * build/gui must be hashes bound to the validated loaded candidate/original GUI.
 * A query response is a point-in-time snapshot, not a promise of future liveness. */
X2dUiReceiptWorker*x2d_ui_receipt_create(const char*build,const char*gui);
/* Production: hash the fixed, currently mapped module on the worker, again
 * before every reply. No receipt if mapping/file identity cannot be proved. */
X2dUiReceiptWorker*x2d_ui_receipt_create_loaded(const char*gui);
int x2d_ui_receipt_take_query(X2dUiReceiptWorker*,X2dUiQuery*);
int x2d_ui_receipt_reply(X2dUiReceiptWorker*,const X2dUiQuery*,int state,uint64_t actual_caps,uint64_t sampled_ms);
void x2d_ui_receipt_invalidate(X2dUiReceiptWorker*);
void x2d_ui_receipt_stop(X2dUiReceiptWorker*);
int x2d_ui_receipt_release(X2dUiReceiptWorker*);
#ifdef __cplusplus
}
#endif
#endif
