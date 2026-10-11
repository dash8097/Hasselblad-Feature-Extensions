#include "rolling_direction.h"
static float absolute(float x){return x<0?-x:x;}
float as_direction(const s32 *pos,const u32 *cv,u32 n,u32 minimum){
    if(n<3 || n>5 || !minimum)return 0;
    /* 每次重看最新窗口；本次无结论不锁存失败，下一个 accepted 继续评估。 */
    u32 first=n-3;float slopes[3],floor=(float)minimum*.005f;
    if(floor<2)floor=2;
    s32 sign=pos[n-1]>pos[first]?1:-1;
    float delta[2];
    for(u32 j=0;j<2;j++){
        u32 i=first+j+1;s32 distance=pos[i]-pos[i-1];
        if(pos[i]<-32768 || pos[i]>32767 || pos[i-1]<-32768 || pos[i-1]>32767 ||
           !cv[i] || !cv[i-1] || distance*sign<=0)return 0;
        delta[j]=(float)cv[i]-(float)cv[i-1];slopes[j]=delta[j]/absolute((float)distance);
    }
    if(!((delta[0]>floor && delta[1]>floor)||(delta[0]<-floor && delta[1]<-floor)))return 0;
    float small=absolute(slopes[0]),large=absolute(slopes[1]);
    if(small>large){float swap=small;small=large;large=swap;}
    if(large>small*4)return 0;
    float slope=(slopes[0]+slopes[1])*.5f;
    if(n>=4){
        s32 distance=pos[first]-pos[first-1];float previous=(float)cv[first]-(float)cv[first-1];
        if(distance*sign>0 && previous*slope>0 && cv[first-1]){
            slopes[2]=previous/absolute((float)distance);
            for(u32 i=1;i<3;i++)for(u32 j=i;j>0 && slopes[j]<slopes[j-1];j--){float swap=slopes[j];slopes[j]=slopes[j-1];slopes[j-1]=swap;}
            slope=slopes[1];
        }
    }
    return (float)sign*slope*absolute((float)(pos[n-1]-pos[first]))/(float)minimum;
}
