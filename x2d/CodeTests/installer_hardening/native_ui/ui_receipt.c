#include "ui_receipt.h"
#include <string.h>
#include <stdio.h>
#include <limits.h>
static int hex(const char*s,size_t n){if(!s)return 0;for(size_t i=0;i<n;i++)if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return 0;return s[n]==0;}
int x2d_ui_query_parse(const char*s,size_t n,char out[33]){if(!s||!out||n!=45||memcmp(s,"UI_QUERY_R1 ",12)||s[44]!='\n')return 0;char v[33];memcpy(v,s+12,32);v[32]=0;if(!hex(v,32))return 0;memcpy(out,v,33);return 1;}
static int number(const char**p,const char*end,uint64_t*out){const char*s=*p;if(s==end||*s<'0'||*s>'9')return 0;uint64_t n=0;const char*begin=s;
 while(s<end&&*s>='0'&&*s<='9'){unsigned d=(unsigned)(*s-'0');if(n>(UINT64_MAX-d)/10)return 0;n=n*10+d;++s;}if(s-begin>1&&*begin=='0')return 0;if(s==end||*s!=' ')return 0;*p=s+1;*out=n;return 1;}
int x2d_ui_receipt_parse(const char*s,size_t n,X2dUiReceipt*out){
 if(!s||!out||n<188||n>512||s[n-1]!='\n'||memchr(s,0,n)||memcmp(s,"UI_NATIVE_R1 ",13))return 0;
 const char*p=s+13,*end=s+n;X2dUiReceipt r;memset(&r,0,sizeof r);uint64_t state=0;
 if(!number(&p,end,&r.pid)||!number(&p,end,&r.start_ticks)||!number(&p,end,&r.generation)||!number(&p,end,&r.sampled_ms)||!number(&p,end,&state)||!number(&p,end,&r.capabilities))return 0;
 if(!r.pid||r.pid>INT_MAX||!r.start_ticks||!r.generation||!r.sampled_ms||state<1||state>4||end-p!=163)return 0;r.state=(int)state;
 memcpy(r.nonce,p,32);if(p[32]!=' ')return 0;p+=33;memcpy(r.build,p,64);if(p[64]!=' ')return 0;p+=65;memcpy(r.gui,p,64);if(p[64]!='\n')return 0;
 if(!hex(r.nonce,32)||!hex(r.build,64)||!hex(r.gui,64))return 0;*out=r;return 1;
}
size_t x2d_ui_receipt_format(const X2dUiReceipt*r,char*out,size_t capacity){
 if(!r||!out||!capacity||!hex(r->nonce,32)||!hex(r->build,64)||!hex(r->gui,64)||!r->pid||r->pid>INT_MAX||!r->start_ticks||!r->generation||!r->sampled_ms||r->state<1||r->state>4)return 0;
 int n=snprintf(out,capacity,"UI_NATIVE_R1 %llu %llu %llu %llu %d %llu %s %s %s\n",(unsigned long long)r->pid,(unsigned long long)r->start_ticks,(unsigned long long)r->generation,(unsigned long long)r->sampled_ms,r->state,(unsigned long long)r->capabilities,r->nonce,r->build,r->gui);
 if(n<0||(size_t)n>=capacity)return 0;return (size_t)n;
}
int x2d_ui_receipt_ready(const X2dUiReceipt*r,const char*nonce,uint64_t pid,uint64_t start,const char*build,const char*gui,uint64_t caps,uint64_t request,uint64_t now){
 return r&&hex(nonce,32)&&hex(build,64)&&hex(gui,64)&&r->state==X2D_UI_READY&&r->pid==pid&&r->start_ticks==start&&r->generation&&caps&&(r->capabilities&caps)==caps&&r->sampled_ms>=request&&r->sampled_ms<=now&&now-r->sampled_ms<=2000&&!strcmp(r->nonce,nonce)&&!strcmp(r->build,build)&&!strcmp(r->gui,gui);
}
