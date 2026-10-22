/* 清理只允许仍为本次独占创建的文件。Windows CRT inode 不足以判定身份。 */
#ifndef FACTORY_PRIVATE_FILE_H
#define FACTORY_PRIVATE_FILE_H
#ifdef _WIN32
#include <windows.h>
#include <io.h>
typedef BY_HANDLE_FILE_INFORMATION FactoryPrivateIdentity;
static int factory_private_identity(int fd,FactoryPrivateIdentity *id){
 return GetFileInformationByHandle((HANDLE)_get_osfhandle(fd),id)!=0;
}
static void factory_private_remove(const char *path,const FactoryPrivateIdentity *id){
 HANDLE h=CreateFileA(path,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
  NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
 if(h==INVALID_HANDLE_VALUE)return;
 FactoryPrivateIdentity now;int same=GetFileInformationByHandle(h,&now)&&
  !(now.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)&&
  now.dwVolumeSerialNumber==id->dwVolumeSerialNumber&&
  now.nFileIndexHigh==id->nFileIndexHigh&&now.nFileIndexLow==id->nFileIndexLow;
 CloseHandle(h);if(same)remove(path);
}
#else
typedef struct stat FactoryPrivateIdentity;
static int factory_private_identity(int fd,FactoryPrivateIdentity *id){return fstat(fd,id)==0;}
static void factory_private_remove(const char *path,const FactoryPrivateIdentity *id){
 struct stat now;
 if(!lstat(path,&now)&&S_ISREG(now.st_mode)&&now.st_dev==id->st_dev&&now.st_ino==id->st_ino)remove(path);
}
#endif
#endif
