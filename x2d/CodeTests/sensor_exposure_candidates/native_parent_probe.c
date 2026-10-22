/* Isolated ARM Linux negative loader: loads the exact production helper,
 * explicitly requests its constructor, and confirms it creates no child. */
extern int setenv(const char*,const char*,int),waitpid(int,int*,int),dlclose(void*);
extern void*dlopen(const char*,int);
extern char*dlerror(void);
extern long write(int,const void*,unsigned long);
extern int*__errno(void);
static int probe(int argc,char**argv,char**env){
 (void)argc;(void)argv;(void)env;
 write(1,"PARENT_PROBE_ENTER\n",19);
 if(setenv("X2D_ESFLASH_CAMERA_CHILD","1",1))return 31;
 write(1,"PARENT_PROBE_ENV\n",17);
 void*library=dlopen("/system/lib64/libx2d_esflash_hotshoe.so",2);if(!library){char*why=dlerror();unsigned n=0;while(why&&why[n]&&n<512)++n;if(n)write(1,why,n);write(1,"\n",1);return 32;}
 int status=0;if(waitpid(-1,&status,1)!=-1||*__errno()!=10)return 33;
 if(dlclose(library))return 34;
 static const char message[]="NATIVE_PARENT_LOADER_REJECTED_UNTRUSTED_HOST\n";
 return write(1,message,sizeof message-1)==sizeof message-1?0:35;
}
typedef struct {void(**preinit)(void),(**init)(void),(**fini)(void);} Structors;
extern __attribute__((noreturn))void __libc_init(void*,void(*)(void),int(*)(int,char**,char**),const Structors*);
__attribute__((noreturn,used))void probe_entry(void*raw){static const Structors s={0};__libc_init(raw,0,probe,&s);}
__attribute__((naked,noreturn))void _start(void){__asm__ volatile("mov x0,sp\nbl probe_entry");}
