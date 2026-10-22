/* 原厂相册输入交接：拍摄前只记录文件身份，拍摄后只读取新增 RAW。
 * 按原厂组序号在进程内交给处理脚本；成功发布后的明确清理入口
 * 只删除私有凭据对应且身份未变的六张输入。不输出照片路径或元数据。 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include "factory_native_3fr.h"
#include "receipt_sha256.h"
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <process.h>
#define fsync _commit
#else
#include <pthread.h>
#endif
#ifndef O_BINARY
#define O_BINARY 0
#endif
#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif
#define LIMIT 65536u
typedef struct {uint64_t device,node,path_hash;} Identity;
static Identity selected[6];
static unsigned frames,mask;
#ifndef FACTORY_NATIVE_DISK_INPUTS
static Identity before[LIMIT];
static unsigned total;
static int collecting;
#endif
static int folder_name(const char *p);
static char sources[6][1024];
typedef struct {Identity id;uint64_t size;char path[1024];unsigned char digest[32];} ReceiptItem;
static ReceiptItem receipt[6];
static const unsigned char receipt_magic[8]={'X','2','D','D','E','L','2',0};
static const unsigned char receipt_key_magic[8]={'X','2','D','K','E','Y','1',0};
/* KEY1 为显式选择的关键区域检查，不能称为完整内容指纹。
 * 老 DEL2 凭据仍全量核验，恢复文件不能悄悄降为抽样。 */
static int key_regions;
_Noreturn static void fail(const char *reason){fprintf(stderr,"NATIVE_ALBUM_INPUTS_REJECTED %s\n",reason);exit(1);}
static int same(Identity a,Identity b){return a.device==b.device&&a.node==b.node;}
static int identity(const char *path,Identity *id,int *directory,uint64_t *size){
#ifdef _WIN32
 DWORD a=GetFileAttributesA(path);
 if(a==INVALID_FILE_ATTRIBUTES||(a&FILE_ATTRIBUTE_REPARSE_POINT))return 0;
 HANDLE h=CreateFileA(path,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
  NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,NULL);
 if(h==INVALID_HANDLE_VALUE)return 0;
 BY_HANDLE_FILE_INFORMATION v;int ok=GetFileInformationByHandle(h,&v)!=0;CloseHandle(h);
 if(!ok||(v.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT))return 0;
 *directory=(v.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=0;
 *id=(Identity){v.dwVolumeSerialNumber,((uint64_t)v.nFileIndexHigh<<32)|v.nFileIndexLow,0};
 *size=((uint64_t)v.nFileSizeHigh<<32)|v.nFileSizeLow;
#else
 struct stat s;if(lstat(path,&s)||(!S_ISDIR(s.st_mode)&&!S_ISREG(s.st_mode)))return 0;
 *directory=S_ISDIR(s.st_mode);*id=(Identity){s.st_dev,s.st_ino,0};*size=(uint64_t)s.st_size;
#endif
 /* exFAT 的 inode 可随缓存回收改变；旧路径摘要也必须排除，不能误读旧照片。
  * 摘要冲突最多导致拒绝缺帧，不会把旧路径当新增输入。 */
 id->path_hash=UINT64_C(14695981039346656037);
 for(const unsigned char *p=(const unsigned char *)path;*p;p++){
  id->path_hash^=*p;id->path_hash*=UINT64_C(1099511628211);
 }
 return 1;
}
static void join(char *out,const char *parent,const char *name){
 int n=snprintf(out,1024,"%s/%s",parent,name);if(n<0||n>=1024)fail("path length");
}
static int is_raw(const char *path){size_t n=strlen(path);return n>4&&
 (!strcmp(path+n-4,".3FR")||!strcmp(path+n-4,".3fr"));}
static void new_raw(const char *path,Identity id,uint64_t size){
 unsigned char b[16384];
 if(size<16384u+210510336u)fail("incomplete new RAW");
 int fd=open(path,O_RDONLY|O_BINARY|O_NOFOLLOW);if(fd<0)fail("new RAW open");
 FILE *f=fdopen(fd,"rb");if(!f){close(fd);fail("new RAW stream");}
 int ok=fread(b,1,sizeof b,f)==sizeof b&&!memcmp(b,"II\052\000",4);fclose(f);
 Identity now;int directory;uint64_t after_size;
 if(!ok||!identity(path,&now,&directory,&after_size)||directory||!same(now,id)||after_size!=size)
  fail("new RAW changed");
 uint32_t first=f3_u32(b+4),raw,exif;
 if(!f3_scalar(b,sizeof b,first,330,&raw)||!f3_scalar(b,sizeof b,first,34665,&exif))fail("RAW directories");
 const unsigned tags[]={256,257,258,259,262,277,273,279};
 const uint32_t values[]={11904,8842,16,1,32803,1,16384,210510336};
 for(unsigned i=0;i<sizeof tags/sizeof tags[0];i++)if(!f3_is(b,sizeof b,raw,tags[i],values[i]))fail("RAW layout");
 uint32_t white=0;
 uint8_t *black=f3_data(b,sizeof b,f3_tag(b,sizeof b,raw,50714),5,1,8);
 if(!black||f3_u32(black+4)!=1||!f3_scalar(b,sizeof b,raw,50717,&white)||
    white<=f3_u32(black)||white>65535)fail("RAW white level");
 uint8_t *maker=f3_tag(b,sizeof b,exif,37500);
 uint8_t *group=maker?f3_data(b,sizeof b,f3_tag(b,sizeof b,f3_u32(maker+8),33),9,4,4):NULL;
 if(!group||f3_u32(group)!=frames)fail("group count");
 unsigned index=f3_u32(group+4);if(index>=frames||(mask&(1u<<index)))fail("group sequence");
 for(unsigned i=0;i<frames;i++)if((mask&(1u<<i))&&same(selected[i],id))fail("duplicate file identity");
 memcpy(sources[index],path,strlen(path)+1);selected[index]=id;mask|=1u<<index;
}
#ifndef FACTORY_NATIVE_DISK_INPUTS
static void scan(const char *root,unsigned depth){
 Identity rid;int directory;uint64_t size;
 if(!identity(root,&rid,&directory,&size)||!directory)fail("directory identity");
 DIR *d=opendir(root);if(!d)fail("directory open");
 struct dirent *entry;
 for(;;){
  errno=0;entry=readdir(d);if(!entry){if(errno)fail("directory read");break;}
  if(!strcmp(entry->d_name,".")||!strcmp(entry->d_name,".."))continue;
  char path[1024];join(path,root,entry->d_name);Identity id;
  if(!identity(path,&id,&directory,&size))fail("unsupported album entry");
  if(directory){if(depth)scan(path,depth-1);continue;}
  if(!collecting){if(total==LIMIT)fail("baseline capacity");before[total++]=id;continue;}
  if(!is_raw(path))continue;
  unsigned i;for(i=0;i<total;i++)if(before[i].path_hash==id.path_hash||same(before[i],id))break;
  if(i==total)new_raw(path,id,size);
 }
 if(closedir(d))fail("directory close");
}
#endif
static unsigned last_folder(const char *root){
 Identity id;int directory;uint64_t size;
 if(!identity(root,&id,&directory,&size)||!directory)fail("last folder root");
 DIR *d=opendir(root);if(!d)fail("last folder open");
 unsigned folder=0;struct dirent *entry;
 for(;;){
  errno=0;entry=readdir(d);if(!entry){if(errno)fail("last folder read");break;}
  if(!folder_name(entry->d_name))continue;
  unsigned n=(unsigned)atoi(entry->d_name);if(n>folder)folder=n;
 }
 if(closedir(d))fail("last folder close");return folder;
}
#ifndef FACTORY_NATIVE_DISK_INPUTS
static void scan_latest(const char *root){
 unsigned folder=last_folder(root);if(!folder)return;
 char name[16],path[1024];snprintf(name,sizeof name,"%03uHASBL",folder);join(path,root,name);
 scan(path,0);
}
#endif
static FILE *exclusive(const char *path){
 int fd=open(path,O_WRONLY|O_CREAT|O_EXCL|O_BINARY|O_NOFOLLOW,0600);
 if(fd<0)fail("output collision");FILE *f=fdopen(fd,"wb");if(!f){close(fd);fail("output stream");}return f;
}
static void synced_close(FILE *f){if(fflush(f)||fsync(fileno(f))||fclose(f))fail("output sync");}
#ifdef FACTORY_NATIVE_DISK_INPUTS
/* 拍摄边界只记录末相册、末编号，不遍历历史相册，不逐张 stat 旧照片。 */
typedef struct {unsigned char magic[8];uint64_t device,root_hash;uint32_t folder,number;} TailBaseline;
static const unsigned char tail_magic[8]={'X','2','D','T','A','I','L','1'};
static int photo_number(const char *p,unsigned *number){
 size_t n=strlen(p);if(n<12||n>13||p[0]<'A'||p[0]>'Z'||p[8]!='.')return 0;
 unsigned value=0;for(unsigned i=1;i<8;i++){if(!isdigit((unsigned char)p[i]))return 0;value=value*10u+(unsigned)(p[i]-'0');}
 char ext[6];size_t bytes=n-8;for(size_t i=0;i<=bytes;i++)ext[i]=(char)toupper((unsigned char)p[8+i]);
 if(strcmp(ext,".3FR")&&strcmp(ext,".JPG")&&strcmp(ext,".HEIC")&&strcmp(ext,".HEIF"))return 0;
 *number=value;return 1;
}
static void tail_folder(const char *root,unsigned folder,char path[1024]){
 char name[16];snprintf(name,sizeof name,"%03uHASBL",folder);join(path,root,name);
 Identity id;int directory;uint64_t size;if(!identity(path,&id,&directory,&size)||!directory)fail("tail folder identity");
}
static void snapshot_tail(const char *root,const char *file){
 TailBaseline b={0};memcpy(b.magic,tail_magic,8);Identity id;int directory;uint64_t size;
 if(!identity(root,&id,&directory,&size)||!directory)fail("tail root identity");
 b.device=id.device;b.root_hash=id.path_hash;b.folder=last_folder(root);
 if(b.folder){
  char path[1024];tail_folder(root,b.folder,path);DIR *d=opendir(path);if(!d)fail("tail folder open");
  for(;;){errno=0;struct dirent *e=readdir(d);if(!e){if(errno)fail("tail folder read");break;}
   unsigned value;if(photo_number(e->d_name,&value)&&value>b.number)b.number=value;
  }
  if(closedir(d))fail("tail folder close");
 }
 FILE *f=exclusive(file);if(fwrite(&b,sizeof b,1,f)!=1)fail("tail baseline write");synced_close(f);
 puts("NATIVE_ALBUM_TAIL_BASELINE_SAVED");
}
static void collect_tail_folder(const char *root,unsigned folder,unsigned cutoff){
 char path[1024];tail_folder(root,folder,path);DIR *d=opendir(path);if(!d)fail("new tail folder open");
 for(;;){errno=0;struct dirent *e=readdir(d);if(!e){if(errno)fail("new tail folder read");break;}
  unsigned number;if(!photo_number(e->d_name,&number)||!is_raw(e->d_name)||number<=cutoff)continue;
  char file[1024];join(file,path,e->d_name);Identity id;int directory;uint64_t size;
  if(!identity(file,&id,&directory,&size)||directory)fail("new tail RAW identity");
  new_raw(file,id,size);
 }
 if(closedir(d))fail("new tail folder close");
}
static void collect_tail(const char *root,const char *file){
 TailBaseline b;int fd=open(file,O_RDONLY|O_BINARY|O_NOFOLLOW);if(fd<0)fail("tail baseline open");
 FILE *f=fdopen(fd,"rb");if(!f){close(fd);fail("tail baseline stream");}
 if(fread(&b,sizeof b,1,f)!=1||fgetc(f)!=EOF||ferror(f)||memcmp(b.magic,tail_magic,8)||
    (b.folder&&(b.folder<100||b.folder>999))||b.number>9999999||(!b.folder&&b.number))fail("tail baseline contract");
 fclose(f);Identity id;int directory;uint64_t size;
 if(!identity(root,&id,&directory,&size)||!directory||id.device!=b.device||id.path_hash!=b.root_hash)fail("tail root changed");
 unsigned latest=last_folder(root);
 if(!latest||latest<b.folder||(b.folder&&latest>b.folder+1))fail("tail album changed unexpectedly");
 if(b.folder)collect_tail_folder(root,b.folder,b.number);
 if(latest!=b.folder)collect_tail_folder(root,latest,0);
}
#endif
static void fingerprint(ReceiptItem *r,unsigned char out[32]){
 Identity id;int directory;uint64_t size;
 if(!identity(r->path,&id,&directory,&size)||directory||id.device!=r->id.device||id.path_hash!=r->id.path_hash||size!=r->size)fail("fingerprint boundary");
 int fd=open(r->path,O_RDONLY|O_BINARY|O_NOFOLLOW);if(fd<0)fail("fingerprint open");
 FILE *f=fdopen(fd,"rb");if(!f){close(fd);fail("fingerprint stream");}
 unsigned char buffer[65536];uint64_t bytes=0;ReceiptSha sha;rsha_init(&sha);size_t n;
 if(key_regions){
  const uint64_t raw_end=UINT64_C(16384)+UINT64_C(210510336);
  if(r->size<raw_end||r->size>INT64_MAX)fail("key region bounds");
  uint64_t offsets[4]={0,16384+210510336/2,raw_end-sizeof buffer,r->size-sizeof buffer};
  rsha_update(&sha,receipt_key_magic,8);
  for(unsigned i=0;i<4;i++){
   unsigned char marker[16];
   for(unsigned j=0;j<8;j++){marker[j]=(unsigned char)(offsets[i]>>(8*j));marker[j+8]=(unsigned char)(r->size>>(8*j));}
#ifdef _WIN32
   if(_fseeki64(f,(__int64)offsets[i],SEEK_SET))fail("key region seek");
#else
   if(fseeko(f,(off_t)offsets[i],SEEK_SET))fail("key region seek");
#endif
   n=fread(buffer,1,sizeof buffer,f);if(n!=sizeof buffer)fail("key region short read");
   rsha_update(&sha,marker,sizeof marker);rsha_update(&sha,buffer,(unsigned)n);bytes+=n;
  }
 }else while((n=fread(buffer,1,sizeof buffer,f))!=0){rsha_update(&sha,buffer,(unsigned)n);bytes+=n;}
 if(ferror(f)||fclose(f)||bytes!=(key_regions?4*sizeof buffer:r->size))fail("fingerprint read");
 Identity after;
 if(!identity(r->path,&after,&directory,&size)||directory||!same(id,after)||size!=r->size)fail("fingerprint changed");
 rsha_final(&sha,out);
}
typedef struct {ReceiptItem *item;unsigned char *digest;} FingerprintJob;
#ifdef _WIN32
static DWORD WINAPI fingerprint_worker(void *opaque){
#else
static void *fingerprint_worker(void *opaque){
#endif
 FingerprintJob *job=opaque;fingerprint(job->item,job->digest);
#ifdef _WIN32
 return 0;
#else
 return NULL;
#endif
}
static uint64_t fingerprint_clock(void){
#ifdef _WIN32
 return GetTickCount64();
#else
 struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))fail("fingerprint clock");
 return (uint64_t)t.tv_sec*1000+(uint64_t)t.tv_nsec/1000000;
#endif
}
static void fingerprint_batch(unsigned char digests[6][32]){
 FingerprintJob jobs[6];uint64_t begin=fingerprint_clock();
#ifdef _WIN32
 HANDLE threads[6];
#else
 pthread_t threads[6];pthread_attr_t attr;
 if(pthread_attr_init(&attr)||pthread_attr_setstacksize(&attr,256*1024))fail("fingerprint thread attributes");
#endif
 for(unsigned i=0;i<6;i++){
  jobs[i]=(FingerprintJob){receipt+i,digests[i]};
#ifdef _WIN32
  threads[i]=CreateThread(NULL,256*1024,fingerprint_worker,jobs+i,0,NULL);
  if(!threads[i])fail("fingerprint thread create");
#else
  if(pthread_create(threads+i,&attr,fingerprint_worker,jobs+i))fail("fingerprint thread create");
#endif
 }
#ifndef _WIN32
 if(pthread_attr_destroy(&attr))fail("fingerprint thread attributes close");
#endif
 for(unsigned i=0;i<6;i++){
#ifdef _WIN32
  if(WaitForSingleObject(threads[i],INFINITE)!=WAIT_OBJECT_0||!CloseHandle(threads[i]))fail("fingerprint thread join");
#else
  if(pthread_join(threads[i],NULL))fail("fingerprint thread join");
#endif
 }
 printf("INPUT_CONTENT_CHECK_THREADS 6\nINPUT_CONTENT_CHECK_MS %llu\n",(unsigned long long)(fingerprint_clock()-begin));fflush(stdout);
 if(key_regions){printf("INPUT_KEY_REGION_CHECK_BYTES %u\n",6u*4u*65536u);fflush(stdout);}
}
static int confined(const char *root,const char *path){
 size_t n=strlen(root);if(strncmp(path,root,n)||path[n]!='/')return 0;
 const char *p=path+n+1;
 if(!strcmp(root+strlen(root)-((strlen(root)>8)?8:0),"/capture"))
  return strlen(p)==5&&p[0]>='0'&&p[0]<='5'&&!strcmp(p+1,".3FR");
 if(strlen(p)!=8+1+12||p[0]<'1'||p[0]>'9'||!isdigit((unsigned char)p[1])||!isdigit((unsigned char)p[2])||memcmp(p+3,"HASBL/",6))return 0;
 for(unsigned i=9;i<17;i++)if(!isalnum((unsigned char)p[i]))return 0;
 return is_raw(path);
}
static void load_receipt(const char *file){
 int fd=open(file,O_RDONLY|O_BINARY|O_NOFOLLOW);if(fd<0)fail("receipt open");
 FILE *f=fdopen(fd,"rb");if(!f){close(fd);fail("receipt stream");}
 unsigned char magic[8];
 if(fread(magic,1,8,f)!=8)fail("receipt contract");
 key_regions=!memcmp(magic,receipt_key_magic,8);
 if((!key_regions&&memcmp(magic,receipt_magic,8))||fread(receipt,sizeof *receipt,6,f)!=6||fgetc(f)!=EOF||ferror(f))fail("receipt contract");
 fclose(f);
}
static void validate_receipt(const char *root){
 Identity id;int directory;uint64_t size;frames=6;mask=0;
 if(!identity(root,&id,&directory,&size)||!directory)fail("receipt root");
 for(unsigned i=0;i<6;i++){
  ReceiptItem *r=receipt+i;
  if(!memchr(r->path,0,sizeof r->path)||!confined(root,r->path))fail("receipt boundary");
  char folder[1024];memcpy(folder,r->path,strlen(r->path)+1);*strrchr(folder,'/')=0;
  if(!identity(folder,&id,&directory,&size)||!directory)fail("receipt folder");
  if(!identity(r->path,&id,&directory,&size)||directory||id.device!=r->id.device||id.path_hash!=r->id.path_hash||size!=r->size)fail("receipt identity");
  new_raw(r->path,id,size);
  r->id=id;
 }
 if(mask!=63)fail("receipt group");
 unsigned char digests[6][32];fingerprint_batch(digests);
 for(unsigned i=0;i<6;i++)if(memcmp(digests[i],receipt[i].digest,32))fail("receipt content changed");
}
static void stage_recorded(const char *root,const char *file,const char *stage){
 load_receipt(file);validate_receipt(root);
 if(strlen(stage)<9||strcmp(stage+strlen(stage)-8,"/capture"))fail("stage boundary");
 char parent[1024];if(strlen(stage)>=sizeof parent)fail("stage length");
 strcpy(parent,stage);*strrchr(parent,'/')=0;
 Identity id;int directory;uint64_t size;
 if(!identity(parent,&id,&directory,&size)||!directory)fail("stage parent");
 char next[1100],original[1100];snprintf(next,sizeof next,"%s.next",file);snprintf(original,sizeof original,"%s.original",file);
#ifdef _WIN32
 if(GetFileAttributesA(next)!=INVALID_FILE_ATTRIBUTES||GetLastError()!=ERROR_FILE_NOT_FOUND)fail("staged receipt collision");
 if(GetFileAttributesA(original)!=INVALID_FILE_ATTRIBUTES||GetLastError()!=ERROR_FILE_NOT_FOUND)fail("original receipt collision");
#else
 struct stat prior;
 if(!lstat(next,&prior)||errno!=ENOENT)fail("staged receipt collision");
 if(!lstat(original,&prior)||errno!=ENOENT)fail("original receipt collision");
#endif
#ifdef _WIN32
 if(mkdir(stage))fail("stage collision");
#else
 if(mkdir(stage,0700))fail("stage collision");
#endif
 ReceiptItem moved[6];memset(moved,0,sizeof moved);unsigned count=0;int valid=1;
 for(unsigned i=0;i<6;i++){
  moved[i]=receipt[i];snprintf(moved[i].path,sizeof moved[i].path,"%s/%u.3FR",stage,i);
  if(rename(receipt[i].path,moved[i].path))break;
  count++;
  if(!identity(moved[i].path,&moved[i].id,&directory,&size)||directory||size!=moved[i].size){valid=0;break;}
 }
 if(count!=6||!valid){
  for(unsigned i=count;i>0;i--)if(rename(moved[i-1].path,receipt[i-1].path))fail("stage rollback");
  if(rmdir(stage))fail("stage rollback directory");fail("stage move");
 }
 FILE *f=exclusive(next);
 if(fwrite(key_regions?receipt_key_magic:receipt_magic,1,8,f)!=8||fwrite(moved,sizeof *moved,6,f)!=6)fail("staged receipt write");
 synced_close(f);
 if(rename(file,original)||rename(next,file))fail("staged receipt commit");
 puts("SIX_INPUTS_STAGED_PRIVATELY");
}
static int folder_name(const char *p){return strlen(p)==8&&p[0]>='1'&&p[0]<='9'&&isdigit((unsigned char)p[1])&&isdigit((unsigned char)p[2])&&!strcmp(p+3,"HASBL");}
static void plan_output(const char *root,const char *file){
 Identity id;int directory;uint64_t size;
 unsigned folder=last_folder(root),max=0;char prefix='B';
 if(folder){
  char name[16],path[1024];snprintf(name,sizeof name,"%03uHASBL",folder);join(path,root,name);
  if(!identity(path,&id,&directory,&size)||!directory)fail("plan folder");
  DIR *sub=opendir(path);if(!sub)fail("plan folder open");struct dirent *e;
  while((e=readdir(sub))){
   const char *p=e->d_name;size_t n=strlen(p);
   if(n<12||p[0]<'A'||p[0]>'Z'||p[8]!='.')continue;
   unsigned value=0;int good=1;
   for(unsigned i=1;i<8;i++){if(!isdigit((unsigned char)p[i])){good=0;break;}value=value*10u+(unsigned)(p[i]-'0');}
   if(!good|| (strcmp(p+8,".3FR")&&strcmp(p+8,".JPG")&&strcmp(p+8,".HEIC")))continue;
   if(value>max){max=value;prefix=p[0];}
  }
  if(closedir(sub))fail("plan folder close");
 }else folder=100;
 if(max>=9999999)fail("plan number range");
 FILE *f=exclusive(file);fprintf(f,"%03uHASBL/%c%07u.JPG\n",folder,prefix,max+1);synced_close(f);
 puts("ONE_FINAL_NUMBER_RESERVED_FROM_EXISTING_ALBUMS");
}
typedef struct {ReceiptItem *item;int ok;} RemovalJob;
#ifdef _WIN32
static DWORD WINAPI removal_worker(void *opaque){
#else
static void *removal_worker(void *opaque){
#endif
 RemovalJob *job=opaque;ReceiptItem *r=job->item;
 Identity id;int directory;uint64_t size;
 if(identity(r->path,&id,&directory,&size)&&!directory&&same(id,r->id)&&size==r->size){
#ifdef FACTORY_TEST_REMOVE_FAIL_INDEX
  if(r!=receipt+FACTORY_TEST_REMOVE_FAIL_INDEX)
#endif
  job->ok=unlink(r->path)==0;
 }
#ifdef _WIN32
 return 0;
#else
 return NULL;
#endif
}
static void remove_recorded(const char *root,const char *file){
 load_receipt(file);validate_receipt(root);
 /* 整组内容已通过校验，再启动独立删除；全部收回后才报告成功。 */
 RemovalJob jobs[6];unsigned started=0;int ok=1;uint64_t begin=fingerprint_clock();
#ifdef _WIN32
 HANDLE threads[6];
#else
 pthread_t threads[6];pthread_attr_t attr;
 if(pthread_attr_init(&attr)||pthread_attr_setstacksize(&attr,256*1024))fail("removal thread attributes");
#endif
 for(unsigned i=0;i<6;i++){
  jobs[i]=(RemovalJob){receipt+i,0};
#ifdef _WIN32
  threads[i]=CreateThread(NULL,256*1024,removal_worker,jobs+i,0,NULL);
  if(!threads[i]){ok=0;break;}
#else
  if(pthread_create(threads+i,&attr,removal_worker,jobs+i)){ok=0;break;}
#endif
  started++;
 }
#ifndef _WIN32
 if(pthread_attr_destroy(&attr))ok=0;
#endif
 for(unsigned i=0;i<started;i++){
#ifdef _WIN32
  if(WaitForSingleObject(threads[i],INFINITE)!=WAIT_OBJECT_0||!CloseHandle(threads[i]))ok=0;
#else
  if(pthread_join(threads[i],NULL))ok=0;
#endif
  if(!jobs[i].ok)ok=0;
 }
 if(!ok||started!=6)fail("input removal");
 printf("INPUT_REMOVE_THREADS 6\nINPUT_REMOVE_MS %llu\n",(unsigned long long)(fingerprint_clock()-begin));
 puts("RECORDED_SIX_INTERMEDIATE_INPUTS_REMOVED");
}
int main(int argc,char **argv){
 (void)factory_native_3fr_header;
#ifndef FACTORY_NATIVE_DISK_INPUTS
 const unsigned char magic[8]={'X','2','D','I','N','P','2',0};
#endif
#ifdef FACTORY_NATIVE_DISK_INPUTS
 /* 本候选保留原厂六张。禁用旧接管流程的源文件删除和迁移入口。 */
 if(argc>1&&(!strcmp(argv[1],"remove-recorded")||!strcmp(argv[1],"stage-recorded")))fail("native sources must be retained");
#endif
 if(argc==4&&!strcmp(argv[1],"plan-output")){plan_output(argv[2],argv[3]);return 0;}
 if(argc==5&&!strcmp(argv[1],"stage-recorded")){stage_recorded(argv[2],argv[3],argv[4]);return 0;}
 if(argc==4&&!strcmp(argv[1],"remove-recorded")){remove_recorded(argv[2],argv[3]);return 0;}
 if(argc==4&&!strcmp(argv[1],"snapshot")){
#ifdef FACTORY_NATIVE_DISK_INPUTS
  snapshot_tail(argv[2],argv[3]);return 0;
#else
  scan_latest(argv[2]);FILE *f=exclusive(argv[3]);uint32_t count=total;
  if(fwrite(magic,1,8,f)!=8||fwrite(&count,4,1,f)!=1||fwrite(before,sizeof *before,total,f)!=total)fail("baseline write");
  synced_close(f);puts("NATIVE_ALBUM_BASELINE_SAVED");return 0;
#endif
 }
 key_regions=argc==8&&!strcmp(argv[1],"process-record-key");
 int recorded=argc==8&&(!strcmp(argv[1],"process-record")||key_regions);
 if((!recorded&&(argc!=7||strcmp(argv[1],"process")))||(strcmp(argv[4],"4")&&strcmp(argv[4],"6")))fail("arguments");
 if(recorded&&strcmp(argv[4],"6"))fail("receipt frame count");
 frames=(unsigned)atoi(argv[4]);FILE *f;
#ifdef FACTORY_NATIVE_DISK_INPUTS
 collect_tail(argv[2],argv[3]);
#else
 unsigned char header[8];uint32_t count;
 int fd=open(argv[3],O_RDONLY|O_BINARY|O_NOFOLLOW);if(fd<0)fail("baseline open");
 f=fdopen(fd,"rb");if(!f){close(fd);fail("baseline stream");}
 if(fread(header,1,8,f)!=8||memcmp(header,magic,8)||fread(&count,4,1,f)!=1||count>LIMIT||
  fread(before,sizeof *before,count,f)!=count||fgetc(f)!=EOF||ferror(f))fail("baseline contract");
 fclose(f);total=count;collecting=1;scan_latest(argv[2]);
#endif
 if(mask!=((1u<<frames)-1))fail("missing new frames");
 int directory;uint64_t size;
 /* SSD 为 exFAT，不依赖硬链接。路径只作为子进程参数，不写入日志或清单。 */
 char *arguments[9]={"sh",argv[recorded?7:6],NULL};
 for(unsigned i=0;i<frames;i++){
  Identity now;
  if(!identity(sources[i],&now,&directory,&size)||directory||!same(now,selected[i]))fail("source replaced");
  arguments[i+2]=sources[i];
  receipt[i]=(ReceiptItem){now,size,{0},{0}};memcpy(receipt[i].path,sources[i],strlen(sources[i])+1);
 }
 if(recorded){
  unsigned char digests[6][32];fingerprint_batch(digests);
  for(unsigned i=0;i<6;i++)memcpy(receipt[i].digest,digests[i],32);
  f=exclusive(argv[5]);
  if(fwrite(key_regions?receipt_key_magic:receipt_magic,1,8,f)!=8||fwrite(receipt,sizeof *receipt,6,f)!=6)fail("receipt write");
  synced_close(f);
 }
 arguments[frames+2]=NULL;
 puts("NATIVE_ALBUM_NEW_GROUP_VERIFIED_ORIGINALS_RETAINED");fflush(stdout);
#ifdef _WIN32
 intptr_t result=_spawnv(_P_WAIT,argv[recorded?6:5],(const char *const *)arguments);
 if(result<0)fail("processor start");return (int)result;
#else
 execv(argv[recorded?6:5],arguments);fail("processor start");
#endif
}
