#ifndef QUARTER_MEMORY_WIRE_H
#define QUARTER_MEMORY_WIRE_H
#include <stdint.h>
#define QM_SEGMENTS 8u
typedef struct {uint64_t handle;uint32_t bytes,allocation;} QmSegment;
typedef struct {uint64_t magic;uint32_t count,reserved;QmSegment segment[QM_SEGMENTS];} QmWire;
#define QM_MAGIC UINT64_C(0x31574d5144325851)
static int qm_wire_valid(const QmWire *w){
 if(!w||w->magic!=QM_MAGIC||!w->count||w->count>QM_SEGMENTS||w->reserved)return 0;
 uint64_t total=0;
 for(unsigned i=0;i<w->count;i++){
  const QmSegment *s=w->segment+i;
  if(!s->handle||!s->bytes||s->bytes%(11904u*2)||s->allocation<s->bytes)return 0;
  for(unsigned j=0;j<i;j++)if(w->segment[j].handle==s->handle)return 0;
  total+=s->bytes;
 }
 for(unsigned i=w->count;i<QM_SEGMENTS;i++)if(w->segment[i].handle||w->segment[i].bytes||w->segment[i].allocation)return 0;
 return total==210510336;
}
#endif
