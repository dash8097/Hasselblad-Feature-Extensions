/* Real Bionic fixture; Qt object functions below are explicit scalar stubs. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "bridge_core.h"
#include "slot_core.h"
struct X2dRectV1 {double x,y,width,height;int present;};
struct X2dEyeStateV1 {int preferred,chosen;double face_id,missing_since,preferred_since,last_time;};
struct X2dEyeResultV1 {struct X2dEyeStateV1 state;int eye,fallback_face;};
extern int x2d_ui_check_existing_runtime_v1(void);
extern int x2d_ui_post_once_v1(UiSlotAction,UiSlotAction,void*);
extern int x2d_nearest_eye_v1(const struct X2dRectV1*,const struct X2dRectV1*,double,double,double,double);
extern int x2d_eye_step_v1(const struct X2dEyeStateV1*,int,double,int,int,double,double,struct X2dEyeResultV1*);
extern void*x2d_native_effect_menu_create_v1(void);
extern void*x2d_native_effect_menu_start_v1(void);
extern void*x2d_native_effect_view_create_v1(void*,void*,void(*)(void*,int),void*);
extern int x2d_native_effect_menu_tick_v1(void*,int,uint64_t);
extern int x2d_native_effect_menu_close_v1(void*);
extern int x2d_native_ui_register_v1(void);
extern unsigned x2d_native_ui_bootstrap_status_v1(void);
extern void*x2d_native_stock_create_v1(unsigned,void*,void*);
static int checks,hits,other_thread;static void*application;
static void*owner_thread(const void*p){hits++;return p?(void*)1:NULL;}
static void*current_thread(void){hits++;return (void*)(uintptr_t)(other_thread?2:1);}
static void exec_stub(void){}
static int callback_hits,release_hits;
static void callback(void*context){callback_hits+=*(int*)context;}
static void release(void*context){release_hits+=*(int*)context;}
static void verify(int truth,const char*label){checks++;if(!truth){fprintf(stderr,"UI_BRIDGE_FAIL %s\n",label);exit(1);}}
static int probe_main(int argc,char**argv,char**envp){
 (void)argv;(void)envp;verify(argc==1,"argc");
 verify(sizeof(struct X2dRectV1)==40&&sizeof(struct X2dEyeStateV1)==40&&sizeof(struct X2dEyeResultV1)==48,"pod-size");
 verify(x2d_ui_check_existing_runtime_v1()==UI_ABI_PROCESS_REJECTED,"wrong-process-before-Qt");
 verify(x2d_native_ui_bootstrap_status_v1()==0,"bootstrap-no-wrong-process-registration");
 verify(x2d_native_ui_register_v1()==UI_ABI_PROCESS_REJECTED&&x2d_native_ui_bootstrap_status_v1()==0,"pre-routine-wrong-process-before-Qt");
 verify(!x2d_native_stock_create_v1(0,(void*)1,(void*)2),"stock-create-wrong-process-before-deref");
 int context=1;
 verify(x2d_ui_post_once_v1(NULL,release,&context)==6,"post-null-callback");
 verify(x2d_ui_post_once_v1(callback,NULL,&context)==6,"post-null-release");
 verify(x2d_ui_post_once_v1(callback,release,&context)==UI_ABI_PROCESS_REJECTED&&!callback_hits&&!release_hits,"post-rejected-before-Qt-and-ownership");
 verify(!x2d_native_effect_menu_create_v1(),"menu-create-wrong-process-before-Qt");
 verify(!x2d_native_effect_menu_start_v1(),"menu-start-wrong-process-before-timer");
 verify(!x2d_native_effect_view_create_v1((void*)1,(void*)2,NULL,NULL),"view-reject-before-pointer-access");
 verify(x2d_native_effect_menu_tick_v1(NULL,1,0)==1,"menu-tick-null");
 verify(x2d_native_effect_menu_close_v1(NULL)==1,"menu-close-null");
 UiSlot*slot=ui_slot_new(callback,release,&context);verify(slot&&slot->references==1&&sizeof(*slot)==40,"slot-layout");
 unsigned char equal=1;slot->impl(2,slot,NULL,NULL,&equal);verify(!equal&&!callback_hits&&!release_hits,"slot-compare");
 slot->impl(1,slot,NULL,NULL,NULL);verify(callback_hits==1&&!release_hits,"slot-call");
 slot->impl(0,slot,NULL,NULL,NULL);verify(release_hits==1,"slot-destroy");
 struct X2dRectV1 left={0.1,0.1,0.1,0.1,1},right={0.8,0.1,0.1,0.1,1};
 verify(x2d_nearest_eye_v1(&left,&right,10,15,100,100)==0,"nearest-left");
 verify(x2d_nearest_eye_v1(&left,&right,90,15,100,100)==1,"nearest-right");
 verify(x2d_nearest_eye_v1(NULL,&right,0,0,1,1)==-1,"null-input");
 left.present=right.present=0;verify(x2d_nearest_eye_v1(&left,&right,0,0,1,1)==-1,"absent-eyes");
 struct X2dEyeStateV1 state={0,0,5,-1,-1,-1};struct X2dEyeResultV1 result={0};
 verify(!x2d_eye_step_v1(&state,0,5,0,1,10,30,&result)&&result.eye==-1,"delay-first");state=result.state;
 verify(!x2d_eye_step_v1(&state,0,5,0,1,41,30,&result)&&result.eye==1,"delay-expired");state=result.state;
 verify(!x2d_eye_step_v1(&state,0,5,0,0,80,30,&result)&&result.eye==-1&&!result.fallback_face,"loss-start");state=result.state;
 verify(!x2d_eye_step_v1(&state,0,5,0,0,111,30,&result)&&result.fallback_face,"loss-face-fallback");
 verify(x2d_eye_step_v1(&state,0,5,2,0,0,30,&result)==1,"invalid-bool");
 UiProfile profile={0};UiCode*code[]={&profile.object_thread,&profile.current_thread,&profile.gui_exec};
 uintptr_t addresses[]={(uintptr_t)owner_thread,(uintptr_t)current_thread,(uintptr_t)exec_stub};Elf64_Phdr segments[4]={0};
 for(int n=0;n<3;n++){code[n]->address=addresses[n];memcpy(code[n]->bytes,(void*)addresses[n],16);segments[n].p_type=PT_LOAD;segments[n].p_vaddr=addresses[n];segments[n].p_memsz=16;segments[n].p_flags=PF_R|PF_X;}
 profile.application_self=(uintptr_t)&application;segments[3].p_type=PT_LOAD;segments[3].p_vaddr=profile.application_self;segments[3].p_memsz=sizeof application;segments[3].p_flags=PF_R|PF_W;UiImage image={0,segments,4};
 verify(ui_check_thread(&image,&profile)==UI_ABI_NO_APP&&!hits,"application-not-created");application=(void*)1;
 verify(ui_check_thread(&image,&profile)==UI_ABI_READY&&hits==2,"scalar-thread-calls");other_thread=1;
 verify(ui_check_thread(&image,&profile)==UI_ABI_WRONG_THREAD&&hits==4,"wrong-thread");
 profile.object_thread.bytes[0]^=1;verify(ui_check_thread(&image,&profile)==UI_ABI_IMAGE_REJECTED&&hits==4,"fingerprint-before-call");profile.object_thread.bytes[0]^=1;
 segments[0].p_flags=PF_R;verify(ui_check_thread(&image,&profile)==UI_ABI_IMAGE_REJECTED,"non-executable");segments[0].p_flags=PF_R|PF_X;
 segments[3].p_flags=PF_R;verify(ui_check_thread(&image,&profile)==UI_ABI_IMAGE_REJECTED,"self-not-writable");segments[3].p_flags=PF_R|PF_W;
 image.base=UINTPTR_MAX;verify(ui_check_thread(&image,&profile)==UI_ABI_IMAGE_REJECTED,"base-overflow");image.base=0;
 image.count=129;verify(ui_check_thread(&image,&profile)==UI_ABI_IMAGE_REJECTED,"excess-segments");
 printf("NATIVE_UI_BIONIC_BRIDGE_PASS assertions=%d Qt_objects=stubs camera_requests=0\n",checks);return 0;
}
typedef struct {void (**preinit)(void),(**init)(void),(**fini)(void);} Structors;
extern __attribute__((noreturn))void __libc_init(void*,void(*)(void),int(*)(int,char**,char**),const Structors*);
__attribute__((noreturn,used))void ui_probe_entry(void*stack){const Structors arrays={0};__libc_init(stack,NULL,probe_main,&arrays);}
__attribute__((naked,noreturn))void _start(void){__asm__ volatile("mov x0,sp\nbl ui_probe_entry");}
