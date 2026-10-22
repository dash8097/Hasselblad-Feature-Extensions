/* 四/六路有界顺序预读。每个线程独占一个 FILE，主线程按原组序消费。
 * 不改变 RAW 数值、颜色或行顺序；失败唤醒消费者，退出时停止并等待线程。 */
#ifndef FOUR_RAW_READER_H
#define FOUR_RAW_READER_H
#ifdef _WIN32
#include <windows.h>
typedef CRITICAL_SECTION ReaderMutex;
typedef CONDITION_VARIABLE ReaderCondition;
typedef HANDLE ReaderThread;
#define READER_ENTRY DWORD WINAPI
#define READER_RETURN 0
static void reader_lock(ReaderMutex *m){EnterCriticalSection(m);}
static void reader_unlock(ReaderMutex *m){LeaveCriticalSection(m);}
static void reader_wake(ReaderCondition *c){WakeAllConditionVariable(c);}
static int reader_wait(ReaderCondition *c,ReaderMutex *m){return SleepConditionVariableCS(c,m,INFINITE)?0:-1;}
#else
#include <pthread.h>
typedef pthread_mutex_t ReaderMutex;
typedef pthread_cond_t ReaderCondition;
typedef pthread_t ReaderThread;
#define READER_ENTRY void *
#define READER_RETURN NULL
static void reader_lock(ReaderMutex *m){pthread_mutex_lock(m);}
static void reader_unlock(ReaderMutex *m){pthread_mutex_unlock(m);}
static void reader_wake(ReaderCondition *c){pthread_cond_broadcast(c);}
static int reader_wait(ReaderCondition *c,ReaderMutex *m){return pthread_cond_wait(c,m);}
#endif
#ifndef FACTORY_READER_BATCH_ROWS
#define FACTORY_READER_BATCH_ROWS 1024u
#endif
#define READER_ROW_SAMPLES 11904u
static unsigned reader_batch_rows=FACTORY_READER_BATCH_ROWS;
typedef struct {
 FILE *file;
 ReaderMutex mutex;
 ReaderCondition changed;
 ReaderThread thread;
 uint16_t *rows;
 unsigned count,produced,consumed,first_row,read_calls;
 int initialized,started,stop,error,done;
} FourRawReader;
static FourRawReader four_readers[6];
static READER_ENTRY four_reader_worker(void *opaque){
 FourRawReader *r=opaque;
 int bad=fseek(r->file,16384L+(long)r->first_row*23808L,SEEK_SET)!=0;
 for(unsigned y=0;!bad&&y<r->count;){
  unsigned count=r->count-y;
  if(count>reader_batch_rows)count=reader_batch_rows;
  reader_lock(&r->mutex);
  while(!r->stop&&r->produced-r->consumed+count>2*reader_batch_rows){
   if(reader_wait(&r->changed,&r->mutex)){bad=1;break;}
  }
  int stop=r->stop;
  reader_unlock(&r->mutex);
  if(stop||bad)break;
  uint16_t *row=r->rows+(y%(2*reader_batch_rows))*READER_ROW_SAMPLES;
  size_t samples=(size_t)count*READER_ROW_SAMPLES;
  r->read_calls++;
  bad=fread(row,2,samples,r->file)!=samples;
  reader_lock(&r->mutex);
  if(!bad){r->produced+=count;y+=count;}
  reader_wake(&r->changed);
  reader_unlock(&r->mutex);
 }
 reader_lock(&r->mutex);
 r->error=bad;r->done=1;reader_wake(&r->changed);
 reader_unlock(&r->mutex);
 return READER_RETURN;
}
static void four_readers_stop(void){
 for(unsigned i=0;i<6;i++){
  FourRawReader *r=four_readers+i;
  if(r->initialized){reader_lock(&r->mutex);r->stop=1;reader_wake(&r->changed);reader_unlock(&r->mutex);}
 }
 for(unsigned i=0;i<6;i++){
  FourRawReader *r=four_readers+i;
  if(r->started){
#ifdef _WIN32
   WaitForSingleObject(r->thread,INFINITE);CloseHandle(r->thread);
#else
   pthread_join(r->thread,NULL);
#endif
   r->started=0;
  }
  if(r->initialized){
#ifdef _WIN32
   DeleteCriticalSection(&r->mutex);
#else
   pthread_cond_destroy(&r->changed);pthread_mutex_destroy(&r->mutex);
#endif
   r->initialized=0;
  }
  free(r->rows);r->rows=NULL;
 }
}
static int four_readers_start(FILE **files,unsigned count,unsigned frame_count){
 if(frame_count!=4&&frame_count!=6)return 0;
 /* 六路双缓冲上限约 279 MiB；余量不足时缩小块，保留至少 96 MiB。 */
#ifndef _WIN32
 FILE *memory=fopen("/proc/meminfo","r");unsigned long available=0;
 if(memory){char line[128];while(fgets(line,sizeof line,memory))
  if(sscanf(line,"MemAvailable: %lu kB",&available)==1)break;
  fclose(memory);
 }
 if(!available&&reader_batch_rows>256)reader_batch_rows=256;
 while(reader_batch_rows>128 &&
       (uint64_t)frame_count*2*reader_batch_rows*READER_ROW_SAMPLES*2+96u*1024u*1024u>(uint64_t)available*1024u)
  reader_batch_rows/=2;
#endif
 for(unsigned i=0;i<frame_count;i++){
  FourRawReader *r=four_readers+i;
  r->file=files[i];r->count=count;r->first_row=93u-(i&1u);
  r->rows=malloc((size_t)2*reader_batch_rows*READER_ROW_SAMPLES*sizeof(uint16_t));
  if(!r->rows)goto failed;
#ifdef _WIN32
  InitializeCriticalSection(&r->mutex);InitializeConditionVariable(&r->changed);
#else
  if(pthread_mutex_init(&r->mutex,NULL))goto failed;
  if(pthread_cond_init(&r->changed,NULL)){pthread_mutex_destroy(&r->mutex);goto failed;}
#endif
  r->initialized=1;
#ifdef _WIN32
  r->thread=CreateThread(NULL,0,four_reader_worker,r,0,NULL);
  if(!r->thread)goto failed;
#else
  if(pthread_create(&r->thread,NULL,four_reader_worker,r))goto failed;
#endif
  r->started=1;
 }
 return 1;
failed:
 four_readers_stop();return 0;
}
static int four_reader_row(unsigned index,unsigned y,uint16_t *output){
 FourRawReader *r=four_readers+index;
 reader_lock(&r->mutex);
 while(!r->error&&!r->done&&r->produced==r->consumed){
  if(reader_wait(&r->changed,&r->mutex)){r->error=1;break;}
 }
 int ok=!r->error&&r->consumed==y&&r->produced>y;
 if(ok){
  memcpy(output,r->rows+(y%(2*reader_batch_rows))*READER_ROW_SAMPLES,READER_ROW_SAMPLES*2);
  r->consumed++;
  if(r->consumed%reader_batch_rows==0||r->consumed==r->count)reader_wake(&r->changed);
 }
 reader_unlock(&r->mutex);return ok;
}
#endif
