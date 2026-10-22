#ifndef X2D_UI_BRIDGE_CORE_H
#define X2D_UI_BRIDGE_CORE_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <elf.h>
typedef struct {uintptr_t address;unsigned char bytes[16];} UiCode;
typedef struct {UiCode object_thread,current_thread,gui_exec;uintptr_t application_self;} UiProfile;
typedef struct {uintptr_t base;const Elf64_Phdr*segments;unsigned count;} UiImage;
enum {UI_ABI_READY=0,UI_ABI_NO_APP=1,UI_ABI_WRONG_THREAD=2,UI_ABI_IMAGE_REJECTED=3,UI_ABI_PROCESS_REJECTED=4,UI_ABI_FILE_REJECTED=5};
static int ui_range(const UiImage*i,uintptr_t relative,size_t bytes,unsigned flags){
 if(!i||!i->segments||!i->count||i->count>128||relative>UINTPTR_MAX-i->base||bytes>UINTPTR_MAX-relative)return 0;
 for(unsigned n=0;n<i->count;n++){const Elf64_Phdr*p=i->segments+n;
  if(p->p_type==PT_LOAD&&(p->p_flags&flags)==flags&&relative>=p->p_vaddr&&p->p_memsz<=UINTPTR_MAX-p->p_vaddr&&relative+bytes<=p->p_vaddr+p->p_memsz&&p->p_vaddr<=UINTPTR_MAX-i->base&&p->p_memsz<=UINTPTR_MAX-i->base-p->p_vaddr)return 1;
 }return 0;
}
static int ui_code(const UiImage*i,const UiCode*c){return ui_range(i,c->address,sizeof c->bytes,PF_R|PF_X)&&!memcmp((const void*)(i->base+c->address),c->bytes,sizeof c->bytes);}
/* Only scalar/pointer calls: no Qt container, QObject layout, QString or moc ABI is guessed. */
static int ui_check_thread(const UiImage*i,const UiProfile*p){
 if(!ui_code(i,&p->object_thread)||!ui_code(i,&p->current_thread)||!ui_code(i,&p->gui_exec)||!ui_range(i,p->application_self,sizeof(void*),PF_R|PF_W))return UI_ABI_IMAGE_REJECTED;
 void*application=NULL;memcpy(&application,(const void*)(i->base+p->application_self),sizeof application);
 if(!application)return UI_ABI_NO_APP;
 void*(*object_thread)(const void*)=(void*(*)(const void*))(i->base+p->object_thread.address);
 void*(*current_thread)(void)=(void*(*)(void))(i->base+p->current_thread.address);
 void*owner=object_thread(application),*current=current_thread();
 return owner&&current&&owner==current?UI_ABI_READY:UI_ABI_WRONG_THREAD;
}
#endif
