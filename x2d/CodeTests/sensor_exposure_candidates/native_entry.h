static int native_main(int argc,char**argv,char**env){
 (void)argc;(void)argv;(void)env;int ok=worker();stop_sampling();
 if(pulse_library){if(!pulse_close||pulse_close()!=0)fail("pulse_close_unconfirmed");else dlclose(pulse_library);}
 return ok&&!failure?0:20;
}
typedef struct {void (**preinit)(void),(**init)(void),(**fini)(void);} NativeStructors;
extern __attribute__((noreturn))void __libc_init(void*,void(*)(void),int(*)(int,char**,char**),const NativeStructors*);
__attribute__((noreturn,used))void native_entry(void*raw){static const NativeStructors s={0};__libc_init(raw,0,native_main,&s);}
__attribute__((naked,noreturn))void _start(void){__asm__ volatile("mov x0,sp\nbl native_entry");}
