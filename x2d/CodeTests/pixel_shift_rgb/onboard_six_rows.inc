/* 原六张离线 GRBG 重建的逐行实现；保留原厂数值域，尚需验证显影 CFA 相位。 */
struct six_half_row { uint16_t values[W][3]; unsigned char valid[W]; };
static struct six_half_row half_rows[2];
#include "six_row_kernel.h"
static void six_half(unsigned y,struct six_half_row *dst) {
 static const unsigned cfa[2][2]={{0,1},{3,2}};
 memset(dst,0,sizeof(*dst));
 for(unsigned i=4;i<6;i++) {
  unsigned sy=y+(i==4?1:0);
  double start=now();
  if(read_mode==2){
#ifdef FACTORY_PARALLEL_READ
   if(!four_reader_row(i,y,input_row))fail("parallel half-grid read");
#else
   fail("parallel reader not built");
#endif
  }else{
   if((read_mode==0||y==0)&&fseek(inputs[i],16384L+(long)(sy+92)*23808L,SEEK_SET))fail("half-grid seek");
   if(fread(input_row,2,11904,inputs[i])!=11904)fail("half-grid row read");
  }
  read_seconds+=now()-start;start=now();
  for(unsigned x=0;x<W;x++) {
   unsigned c=cfa[sy%2][(x+1)%2];if(c==3)c=1;
   if(dst->valid[x]&(1u<<c))fail("duplicate half-grid channel");
   dst->values[x][c]=input_row[125+x];dst->valid[x]|=(unsigned char)(1u<<c);
  }
  normalize_seconds+=now()-start;
 }
 for(unsigned x=0;x<W;x++)if(dst->valid[x]!=3 && dst->valid[x]!=6)fail("incomplete half-grid");
}
struct SixCompute {uint16_t (*a)[4],(*b)[4];struct six_half_row *cur,*prev;};
static void six_compute(unsigned begin,unsigned end,void *opaque){
 struct SixCompute *c=opaque;uint16_t (*a)[4]=c->a,(*b)[4]=c->b;
 struct six_half_row *cur=c->cur,*prev=c->prev;
 SixNativeRow task={W,(const uint16_t(*)[4])a,(const uint16_t(*)[4])b,
  (const uint16_t(*)[3])cur->values,(const uint16_t(*)[3])prev->values,
  cur->valid,prev->valid,output_rows};
 six_native_range(begin,end,&task);
}
static void six_prepare_rows(unsigned y){
 six_half(y,&half_rows[y%2]);
}
static void six_output_rows(unsigned y,uint16_t a[W][4],uint16_t b[W][4]) {
 struct six_half_row *cur=&half_rows[y%2],*prev=&half_rows[(y+1)%2];
 if(y==0)prev=cur;
 struct SixCompute task={a,b,cur,prev};
#ifdef FACTORY_PARALLEL_READ
 if(compute_threads==4)merge_workers_run(W,six_compute,&task);else
#endif
 six_compute(0,W,&task);
}
