/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hasselblad Feature Extensions contributors
 */
/* 单个写入线程独占输出 FILE；有界队列保持行序，失败唤醒生产者。 */
#ifndef BACKGROUND_WRITER_H
#define BACKGROUND_WRITER_H
#define WRITER_ROWS 128u
static struct {
 ReaderMutex mutex;
 ReaderCondition changed;
 ReaderThread thread;
 FILE *file;
 uint16_t *buffer;
 unsigned samples,produced,consumed;
 int initialized,started,stop,done,error;
 double io_seconds;
} output_writer;
static READER_ENTRY output_writer_worker(void *opaque){
 (void)opaque;
 reader_lock(&output_writer.mutex);
 for(;;){
  while(!output_writer.stop&&!output_writer.done&&output_writer.produced==output_writer.consumed)
   if(reader_wait(&output_writer.changed,&output_writer.mutex)){output_writer.error=1;output_writer.stop=1;}
  if(output_writer.stop||(output_writer.done&&output_writer.produced==output_writer.consumed))break;
  unsigned slot=output_writer.consumed%WRITER_ROWS;
  unsigned count=output_writer.produced-output_writer.consumed;
  if(count>WRITER_ROWS-slot)count=WRITER_ROWS-slot;
  size_t samples=(size_t)count*output_writer.samples;
  reader_unlock(&output_writer.mutex);
  double start=now();
#ifdef FACTORY_TEST_WRITER_FAIL_AFTER_ROWS
  int bad=output_writer.consumed>=FACTORY_TEST_WRITER_FAIL_AFTER_ROWS;
  if(!bad)bad=fwrite(output_writer.buffer+(size_t)slot*output_writer.samples,2,samples,output_writer.file)!=samples;
#else
  int bad=fwrite(output_writer.buffer+(size_t)slot*output_writer.samples,2,samples,output_writer.file)!=samples;
#endif
  output_writer.io_seconds+=now()-start;
  reader_lock(&output_writer.mutex);
  if(bad){output_writer.error=1;output_writer.stop=1;}
  else output_writer.consumed+=count;
  reader_wake(&output_writer.changed);
 }
 reader_wake(&output_writer.changed);reader_unlock(&output_writer.mutex);return READER_RETURN;
}
static void output_writer_stop(void){
 if(output_writer.initialized){
  reader_lock(&output_writer.mutex);output_writer.stop=1;
  reader_wake(&output_writer.changed);reader_unlock(&output_writer.mutex);
 }
 if(output_writer.started){
#ifdef _WIN32
  WaitForSingleObject(output_writer.thread,INFINITE);CloseHandle(output_writer.thread);
#else
  pthread_join(output_writer.thread,NULL);
#endif
  output_writer.started=0;
 }
 if(output_writer.initialized){
#ifdef _WIN32
  DeleteCriticalSection(&output_writer.mutex);
#else
  pthread_cond_destroy(&output_writer.changed);pthread_mutex_destroy(&output_writer.mutex);
#endif
  output_writer.initialized=0;
 }
 free(output_writer.buffer);output_writer.buffer=NULL;
}
static int output_writer_start(FILE *file,unsigned samples){
 output_writer.file=file;output_writer.samples=samples;
 output_writer.buffer=malloc((size_t)WRITER_ROWS*samples*2);
 if(!output_writer.buffer)return 0;
#ifdef _WIN32
 InitializeCriticalSection(&output_writer.mutex);InitializeConditionVariable(&output_writer.changed);
#else
 if(pthread_mutex_init(&output_writer.mutex,NULL))goto failed;
 if(pthread_cond_init(&output_writer.changed,NULL)){pthread_mutex_destroy(&output_writer.mutex);goto failed;}
#endif
 output_writer.initialized=1;
#ifdef _WIN32
 output_writer.thread=CreateThread(NULL,0,output_writer_worker,NULL,0,NULL);
 if(!output_writer.thread)goto failed;
#else
 if(pthread_create(&output_writer.thread,NULL,output_writer_worker,NULL))goto failed;
#endif
 output_writer.started=1;return 1;
failed:
 output_writer_stop();return 0;
}
static int output_writer_put(const uint16_t *row){
 reader_lock(&output_writer.mutex);
 while(!output_writer.error&&output_writer.produced-output_writer.consumed==WRITER_ROWS)
  if(reader_wait(&output_writer.changed,&output_writer.mutex)){output_writer.error=1;break;}
 int ok=!output_writer.error;
 if(ok){
  memcpy(output_writer.buffer+(size_t)(output_writer.produced%WRITER_ROWS)*output_writer.samples,row,output_writer.samples*2);
  output_writer.produced++;reader_wake(&output_writer.changed);
 }
 reader_unlock(&output_writer.mutex);return ok;
}
static int output_writer_finish(void){
 reader_lock(&output_writer.mutex);output_writer.done=1;reader_wake(&output_writer.changed);
 while(!output_writer.error&&output_writer.consumed!=output_writer.produced)
  if(reader_wait(&output_writer.changed,&output_writer.mutex)){output_writer.error=1;break;}
 int ok=!output_writer.error;reader_unlock(&output_writer.mutex);
 output_writer_stop();return ok;
}
#endif
