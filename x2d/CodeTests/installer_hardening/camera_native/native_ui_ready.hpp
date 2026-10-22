#pragma once
#include <cstdint>
namespace cn {
struct UiReadyContract {const char*module_sha256;const char*gui_sha256;uint64_t capabilities;};
// Fresh request/reply snapshot of a single current GUI incarnation. Does not
// promise future liveness, and never restarts a service or repeats a UI action.
void native_ui_ready(const UiReadyContract&,unsigned timeout_ms=15000);
}
