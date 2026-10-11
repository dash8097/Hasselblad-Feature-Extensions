/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hasselblad Feature Extensions contributors
 */
/* 固定计算线程池；调用返回前所有区段完成，写盘仍按原行序进行。 */
#ifndef MERGE_WORKERS_H
#define MERGE_WORKERS_H
typedef void (*MergeRange)(unsigned,unsigned,void *);
static struct {
 ReaderMutex mutex;
 ReaderCondition changed;
 ReaderThread threads[4];
 unsigned ids[4],started,generation,finished,width;
 int initialized,stop;
 MergeRange run;
 void *context;
} merge_pool;
static READER_ENTRY merge_worker(void *opaque){
 unsigned id=*(unsigned *)opaque,seen=0;
 reader_lock(&merge_pool.mutex);
 for(;;){
  while(!merge_pool.stop&&seen==merge_pool.generation)
   if(reader_wait(&merge_pool.changed,&merge_pool.mutex))abort();
  if(merge_pool.stop)break;
  seen=merge_pool.generation;
  unsigned begin=merge_pool.width*id/4,end=merge_pool.width*(id+1)/4;
  MergeRange run=merge_pool.run;void *context=merge_pool.context;
  reader_unlock(&merge_pool.mutex);
  run(begin,end,context);
  reader_lock(&merge_pool.mutex);
  merge_pool.finished++;reader_wake(&merge_pool.changed);
 }
 reader_unlock(&merge_pool.mutex);return READER_RETURN;
}
static void merge_workers_stop(void){
 if(!merge_pool.initialized)return;
 reader_lock(&merge_pool.mutex);merge_pool.stop=1;
 reader_wake(&merge_pool.changed);reader_unlock(&merge_pool.mutex);
 for(unsigned i=0;i<merge_pool.started;i++){
#ifdef _WIN32
  WaitForSingleObject(merge_pool.threads[i],INFINITE);CloseHandle(merge_pool.threads[i]);
#else
  pthread_join(merge_pool.threads[i],NULL);
#endif
 }
#ifdef _WIN32
 DeleteCriticalSection(&merge_pool.mutex);
#else
 pthread_cond_destroy(&merge_pool.changed);pthread_mutex_destroy(&merge_pool.mutex);
#endif
 merge_pool.initialized=0;merge_pool.started=0;
}
static int merge_workers_start(void){
#ifdef _WIN32
 InitializeCriticalSection(&merge_pool.mutex);InitializeConditionVariable(&merge_pool.changed);
#else
 if(pthread_mutex_init(&merge_pool.mutex,NULL))return 0;
 if(pthread_cond_init(&merge_pool.changed,NULL)){pthread_mutex_destroy(&merge_pool.mutex);return 0;}
#endif
 merge_pool.initialized=1;
 for(unsigned i=0;i<4;i++){
  merge_pool.ids[i]=i;
#ifdef _WIN32
  merge_pool.threads[i]=CreateThread(NULL,0,merge_worker,&merge_pool.ids[i],0,NULL);
  if(!merge_pool.threads[i])goto failed;
#else
  if(pthread_create(&merge_pool.threads[i],NULL,merge_worker,&merge_pool.ids[i]))goto failed;
#endif
  merge_pool.started++;
 }
 return 1;
failed:
 merge_workers_stop();return 0;
}
static void merge_workers_run(unsigned width,MergeRange run,void *context){
 reader_lock(&merge_pool.mutex);
 merge_pool.width=width;merge_pool.run=run;merge_pool.context=context;
 merge_pool.finished=0;merge_pool.generation++;reader_wake(&merge_pool.changed);
 while(merge_pool.finished<4)
  if(reader_wait(&merge_pool.changed,&merge_pool.mutex))abort();
 reader_unlock(&merge_pool.mutex);
}
#endif
