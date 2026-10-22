/* Fixed Qt 6.4.1 QSlotObjectBase scalar ABI, corroborated in GUI 4.2.0.
 * Allocate and release in this module, never pass a foreign new/delete pair.
 * A scheduled callback and its context must remain resident until release.
 */
#ifndef X2D_NATIVE_SLOT_CORE_H
#define X2D_NATIVE_SLOT_CORE_H
#include <stddef.h>
#include <stdlib.h>
typedef struct UiSlot UiSlot;
typedef void (*UiSlotAction)(void*);
struct UiSlot {
 int references;
 void (*impl)(int,UiSlot*,void*,void**,unsigned char*);
 UiSlotAction callback,release;
 void*context;
};
#if defined(__cplusplus)
static_assert(offsetof(UiSlot,impl)==8,"Qt ARM64 slot impl offset");
#else
_Static_assert(offsetof(UiSlot,impl)==8,"Qt ARM64 slot impl offset");
#endif
static void ui_slot_impl(int operation,UiSlot*slot,void*receiver,void**arguments,unsigned char*result){
 (void)receiver;(void)arguments;
 if(operation==0){if(slot->release)slot->release(slot->context);free(slot);}
 else if(operation==1){if(slot->callback)slot->callback(slot->context);}
 else if(operation==2&&result)*result=0;
}
static UiSlot*ui_slot_new(UiSlotAction callback,UiSlotAction release,void*context){
 if(!callback)return NULL;
 UiSlot*slot=(UiSlot*)malloc(sizeof(UiSlot));if(!slot)return NULL;
 slot->references=1;slot->impl=ui_slot_impl;slot->callback=callback;slot->release=release;slot->context=context;return slot;
}
#endif
