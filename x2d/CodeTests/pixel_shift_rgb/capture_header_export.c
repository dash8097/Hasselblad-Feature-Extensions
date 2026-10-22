#include <string.h>
#include "capture_header_export.h"
#include "factory_native_3fr.h"
typedef struct {uintptr_t address,parent;uint64_t low,high;} ChNode;
static int read_at(ChReadApi api,uintptr_t address,void *out,size_t bytes){
 return address&&bytes<=UINTPTR_MAX-address&&api.read(api.context,address,out,bytes);
}
static int first_identity(uint8_t *header,uint32_t main,uint32_t raw){
 uint8_t *entry=f3_tag(header,16384,raw,37500);uint32_t exif;
 if(!entry)entry=f3_tag(header,16384,main,37500);
 if(!entry&&f3_scalar(header,16384,main,34665,&exif))entry=f3_tag(header,16384,exif,37500);
 if(!entry)return 0;
 uint32_t maker=f3_u32(entry+8);
 uint8_t *group=f3_data(header,16384,f3_tag(header,16384,maker,33),9,4,4);
 return group&&f3_u32(group)==6&&f3_u32(group+4)==0;
}
static int export_header(ChReadApi api,uintptr_t work,uint8_t out[16384],int six){
 if(!api.read||!api.allowed||!api.allowed(api.context)||!work||!out||work>UINTPTR_MAX-0x58)return 0;
 uint8_t type;uintptr_t data;
 if(!read_at(api,work+0x38,&type,1)||type!=1||!read_at(api,work+0x50,&data,8)||!data||
    data>UINTPTR_MAX-0x20)return 0;
 uint8_t map[32];if(!read_at(api,data,map,sizeof map))return 0;
 uint64_t count;uintptr_t root;memcpy(&count,map+24,8);memcpy(&root,map+16,8);
 if(!count||count>64||!root)return 0;
 uint8_t result[16384]={0},covered[16384]={0};uintptr_t visited[64];unsigned used=0,queued=1;
 ChNode stack[64]={{root,data+16,0,UINT64_C(1)<<32}};
 while(queued){
  ChNode next=stack[--queued];
  if(used>=count)return 0;
  for(unsigned i=0;i<used;i++)if(visited[i]==next.address)return 0;
  visited[used++]=next.address;
  uint8_t node[88];if(!read_at(api,next.address,node,sizeof node))return 0;
  uintptr_t left,right,parent,bytes;intptr_t length;
  memcpy(&left,node,8);memcpy(&right,node+8,8);memcpy(&parent,node+16,8);
  uint32_t offset=f3_u32(node+32);
  memcpy(&bytes,node+72,8);memcpy(&length,node+80,8);
  if(parent!=next.parent||offset<next.low||offset>=next.high||length<0||(!bytes&&length))return 0;
  if(left){if(queued==64)return 0;stack[queued++]=(ChNode){left,next.address,next.low,offset};}
  if(right){if(queued==64)return 0;stack[queued++]=(ChNode){right,next.address,(uint64_t)offset+1,next.high};}
  /* 后面的预览等片段不是 RAW 头部；不触碰其数据。 */
  if(offset>=16384||!length)continue;
  size_t n=(uint64_t)length>16384-offset?16384-offset:(size_t)length;
  for(size_t i=0;i<n;i++)if(covered[offset+i])return 0;
  if(!read_at(api,bytes,result+offset,n))return 0;
  memset(covered+offset,1,n);
 }
 if(used!=count)return 0;
 for(unsigned i=0;i<8;i++)if(!covered[i])return 0;
 uint32_t main=f3_u32(result+4),raw;
 F3Occupied occupied={0};memset(occupied.used,1,8);
 if(!f3_mark_ifd(result,main,&occupied))return 0;
 for(unsigned i=0;i<16384;i++)if(occupied.used[i]&&!covered[i])return 0;
 if(memcmp(result,"II*\0",4)||!f3_scalar(result,16384,main,330,&raw)||
    !f3_is(result,16384,raw,256,11904)||!f3_is(result,16384,raw,257,8842)||
    !f3_is(result,16384,raw,258,16)||!f3_is(result,16384,raw,259,1)||
    !f3_is(result,16384,raw,273,16384)||!f3_is(result,16384,raw,279,210510336)||
    (six&&!first_identity(result,main,raw)))return 0;
 memcpy(out,result,sizeof result);return 1;
}
int ch_export_first_header(ChReadApi api,uintptr_t work,uint8_t out[16384]){return export_header(api,work,out,1);}
int ch_export_owned_single_header(ChReadApi api,uintptr_t work,uint8_t out[16384]){return export_header(api,work,out,0);}
