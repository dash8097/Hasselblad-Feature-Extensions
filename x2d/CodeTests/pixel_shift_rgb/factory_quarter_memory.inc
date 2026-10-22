#include "quarter_memory_wire.h"
static int quarter_marker(const char *job,const char *leaf,const char *value){
 char p[192];if(!stack_path(p,job,leaf))return 0;
 int fd=(int)sc(56,-100,(intptr_t)p,0xa00c1,0600);if(fd<0)return 0;
 size_t n=strlen(value);int ok=sc(64,fd,(intptr_t)value,n,0)==(long)n&&sc(82,fd,0,0,0)==0;
 if(sc(57,fd,0,0,0))ok=0;return ok;
}
static int quarter_allowed(const char *job){
 char p[192];
 for(unsigned i=0;i<1800;i++){
  if(!stack_path(p,job,"quarter.render.abort"))return 0;
  int fd=(int)sc(56,-100,(intptr_t)p,0xa0000,0);if(fd>=0){sc(57,fd,0,0,0);return 0;}
  if(!stack_path(p,job,"quarter.render.allow"))return 0;
  fd=(int)sc(56,-100,(intptr_t)p,0xa0000,0);
  if(fd>=0){char marker[18];long n=sc(63,fd,(intptr_t)marker,sizeof marker,0);sc(57,fd,0,0,0);
   return n==17&&!memcmp(marker,"QUARTER_ALLOW_R1\n",17);
  }
  long delay[2]={0,100000000};sc(101,(intptr_t)delay,0,0,0);
 }
 return 0;
}
static int quarter_copy_shared(void *ion,void *destination,void *map,const char *job){
 char p[192];QmWire wire;int fd=-1,ok=0;void *imports[QM_SEGMENTS]={0},*maps[QM_SEGMENTS]={0};
 if(!stack_path(p,job,"raw.memory"))goto done;
 fd=(int)sc(56,-100,(intptr_t)p,0xa0000,0);
 if(fd<0||sc(62,fd,0,2,0)!=(long)sizeof wire||!stack_read(fd,&wire,sizeof wire,0)||!qm_wire_valid(&wire))goto done;
 if(sc(57,fd,0,0,0))goto done;fd=-1;
 uint64_t at=0;
 for(unsigned i=0;i<wire.count;i++){
  uint32_t actual=0;
  if(duss_hal_mem_import(ion,imports+i,wire.segment[i].handle)||!imports[i]||
     duss_hal_mem_get_size(imports[i],&actual)||actual!=wire.segment[i].allocation||
     duss_hal_mem_map(imports[i],maps+i)||!maps[i]||duss_hal_mem_sync(imports[i],FACTORY_SYNC_CPU))goto done;
  memcpy((uint8_t*)map+at,maps[i],wire.segment[i].bytes);at+=wire.segment[i].bytes;
 }
 if(at!=210510336||duss_hal_mem_sync(destination,FACTORY_SYNC_DEVICE))goto done;
 ok=1;
done:
 if(fd>=0)sc(57,fd,0,0,0);
 for(unsigned i=QM_SEGMENTS;i-->0;){
  if(maps[i]&&duss_hal_mem_unmap(imports[i]))ok=0;
  if(imports[i]&&duss_hal_mem_free(imports[i]))ok=0;
 }
 if(ok&&!quarter_marker(job,"quarter.memory.copied","QUARTER_COPIED_R1\n"))ok=0;
 if(ok)text("QUARTER_RAW_IMPORTED_FROM_MEMORY_NO_SOURCE_FILE_READ\n");return ok;
}
