#ifndef AF_POLICY_H
#define AF_POLICY_H
#include "native_af.h"
enum { AP_PROBE, AP_FAST, AP_FINE, AP_DYNAMIC_70=65533 };
struct AfPolicy {u32 eligible,start_speed,start_samples,probe,fast,fine,far_first,fine_advance_ms;
    u32 identity_token,identity_key,reported_speed;};
u32 af_policy_begin(struct AfPolicy *,u32 generation);
int af_policy_current(const struct AfPolicy *);
u32 af_reported_start_speed(void);
u32 af_identity_token(u32 *key);
u32 af_current_advance(void);
void af_window_reset(u32 generation);
void af_window_sample(u32 count);
#endif
