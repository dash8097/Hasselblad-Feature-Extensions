#include "native_feature_contract.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static unsigned checks;
static void check(int x){++checks;if(!x){fprintf(stderr,"FEATURE_CONTRACT_FAIL %u\n",checks);exit(1);}}
int main(void){
 check(x2d_native_feature_mask("248")==248);check(x2d_native_feature_mask("255")==255);check(!x2d_native_features_advanced(248));check(x2d_native_features_advanced(255));check(x2d_native_required_capabilities(248)==4081);check(x2d_native_required_capabilities(255)==4095);check(!x2d_native_required_capabilities(7));
 check(!x2d_native_features_valid(NULL));check(!x2d_native_features_valid(""));
 for(unsigned n=0;n<4096;n++){char s[20];snprintf(s,sizeof s,"%u",n);check(x2d_native_features_valid(s)==(n==248||n==255));}
 const char*bad[]={" 255","255 ","255\n","255\r","255\t","0255","+255","-255","0xff","255.0","2","25","2555","ALL","true"};
 for(unsigned i=0;i<sizeof bad/sizeof bad[0];i++)check(!x2d_native_features_valid(bad[i]));
 for(unsigned pos=0;pos<3;pos++)for(unsigned c=1;c<256;c++){char s[]="255";s[pos]=(char)c;check(x2d_native_features_valid(s)==(c==(unsigned char)"255"[pos]));}
 for(unsigned c=1;c<256;c++){char s[5]={'2','5','5',(char)c,0};check(!x2d_native_features_valid(s));}
 printf("NATIVE_FEATURE_CONTRACT_PASS assertions=%u selected_masks=248,255 no_environment_defaults=1\n",checks);
}
