/* 仅在本包私有目录验证目标 Linux socket/stat/credentials ABI；不连消息服务或 UART。 */
#define AS_DIRECTORY "/tmp/hbl-af-bus-r4/local-check"
#include "settings_socket.h"
#include <cstdio>
int main(int argc,char **){
    if(argc!=1 || geteuid()!=0)return 20;
    if(mkdir(AS_DIRECTORY,0700))return 21;
    int a=as_bind(AS_BACKEND),b=as_bind(AS_UI),result=0;unsigned reason=0;
    unsigned char sent[255]={},received[255]={};
    for(unsigned i=0;i<255;i++)sent[i]=static_cast<unsigned char>(i);
    if(a<0 || b<0)result=22;
    else if(!as_send(b,AS_BACKEND,sent))result=23;
    else if(as_recv(a,received,AS_UI,&reason)!=1 || std::memcmp(sent,received,255))result=24;
    else if(!as_send(a,AS_UI,sent))result=25;
    else if(as_recv(b,received,AS_BACKEND,&reason)!=1 || std::memcmp(sent,received,255))result=26;
    if(a>=0){close(a);if(unlink(AS_BACKEND))result=27;}
    if(b>=0){close(b);if(unlink(AS_UI))result=27;}
    if(rmdir(AS_DIRECTORY))result=28;
    std::printf("local-socket-check result=%d reason=%u\n",result,reason);
    return result;
}
