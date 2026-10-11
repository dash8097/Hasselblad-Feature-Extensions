#include "native_af.h"
static float absf(float x){return x<0?-x:x;}
static float maxf(float a,float b){return a>b?a:b;}
static float minf(float a,float b){return a<b?a:b;}
static int finitef(float x){return x==x && x<3.0e30f && x>-3.0e30f;}
static float median(float *p,u32 n){
    for(u32 i=1;i<n;i++){float v=p[i];u32 j=i;while(j && p[j-1]>v){p[j]=p[j-1];j--;}p[j]=v;}
    return n&1?p[n/2]:(p[n/2-1]+p[n/2])*0.5f;
}
static float root(float x){
    union {float f;u32 w;} v={.f=x};
    if(x<=0)return 0;
    v.w=(v.w>>1)+0x1fc00000;
    for(u32 i=0;i<6;i++)v.f=0.5f*(v.f+x/v.f);
    return v.f;
}
static float limited_ratio(float speed,float horizon,float braking,float room){
    float at=braking*horizon;room=maxf(0,room);
    float safe=2*braking*room/(root(at*at+2*braking*room)+at);
    return minf(1,maxf(0,safe/speed));
}
/* x 已缩放至 [-1,0]；避免直接以镜头绝对位置求解病态矩阵。 */
static int quadratic(const float *x,const float *y,u32 n,float *b,float *residual){
    float a[3][4]={{0}};
    for(u32 i=0;i<n;i++){
        float p[5]={1,x[i],x[i]*x[i],0,0};p[3]=p[2]*p[1];p[4]=p[2]*p[2];
        for(u32 j=0;j<3;j++){for(u32 k=0;k<3;k++)a[j][k]+=p[j+k];a[j][3]+=p[j]*y[i];}
    }
    for(u32 k=0;k<3;k++){
        u32 best=k;for(u32 j=k+1;j<3;j++)if(absf(a[j][k])>absf(a[best][k]))best=j;
        if(absf(a[best][k])<0.00001f)return 0;
        for(u32 j=k;j<4;j++){float t=a[k][j];a[k][j]=a[best][j];a[best][j]=t;}
        float d=a[k][k];for(u32 j=k;j<4;j++)a[k][j]/=d;
        for(u32 i=0;i<3;i++)if(i!=k){float f=a[i][k];for(u32 j=k;j<4;j++)a[i][j]-=f*a[k][j];}
    }
    for(u32 k=0;k<3;k++)b[k]=a[k][3];
    *residual=0;
    for(u32 i=0;i<n;i++)*residual=maxf(*residual,absf(y[i]-(b[0]+b[1]*x[i]+b[2]*x[i]*x[i])));
    return finitef(b[0]) && finitef(b[1]) && finitef(b[2]);
}

void na_assess(const struct NaSample *s,u32 n,const struct NaTiming *t,struct NaResult *o){
    *o=(struct NaResult){.reason=NA_TOO_FEW,.safe_speed_ratio=1};
    if(n<2 || n>5)return;
    float intervals[4],speeds[4],steps[4],x[5],y[5],b[3],noise;
    for(u32 i=0;i<n;i++)if(s[i].position<-32768 || s[i].position>32767){o->reason=NA_METADATA;return;}
    s32 movement=s[n-1].position-s[0].position;
    if(!movement){o->reason=NA_MOTION;return;}
    float direction=movement>0?1.0f:-1.0f,span=absf((float)movement);
    float minimum=(float)s[0].cv,maximum=minimum,slowest=3.0e30f,fastest=0;
    for(u32 i=0;i<n;i++){
        if((s[i].flags&NA_REQUIRED)!=NA_REQUIRED || (s[i].flags&~15u) || s[i].flags!=s[0].flags ||
           ((s[i].flags&NA_SECONDARY) && !s[i].secondary_cv) || !s[i].cv || s[i].position<-32768 || s[i].position>32767 ||
           s[i].generation!=s[0].generation || (i && ((u32)(s[i].frame_id-s[i-1].frame_id)!=1 ||
           s[i].native_count!=s[i-1].native_count+1))){o->reason=NA_METADATA;return;}
        if((u32)(s[i].receive_tick-s[i].sample_tick)>0x7fffffff ||
           (u32)(t->now-s[i].receive_tick)>0x7fffffff){o->reason=NA_TIME;return;}
        if(i){
            u32 dt=s[i].sample_tick-s[i-1].sample_tick;
            s32 dp=s[i].position-s[i-1].position;
            if(!dt || dt>0x7fffffff || (u32)(s[i].receive_tick-s[i-1].receive_tick)>0x7fffffff){o->reason=NA_TIME;return;}
            if((float)dp*direction<=0){o->reason=NA_MOTION;return;}
            intervals[i-1]=(float)dt;steps[i-1]=absf((float)dp);speeds[i-1]=steps[i-1]/(float)dt;
            slowest=minf(slowest,speeds[i-1]);fastest=maxf(fastest,speeds[i-1]);
        }
        minimum=minf(minimum,(float)s[i].cv);maximum=maxf(maximum,(float)s[i].cv);
        x[i]=((float)s[i].position-s[n-1].position)*direction/span;
    }
    float period=median(intervals,n-1),speed=median(speeds,n-1),step=median(steps,n-1);
    float age=(float)(u32)(t->now-s[n-1].sample_tick);
    o->used=n;o->velocity_per_tick=direction*speed;o->frame_ticks=period;o->age_ticks=age;o->position_step=step;
    if(intervals[n-2]>period*3 || age>period*4 || (float)t->processing_ticks>period*2 ||
       (float)t->command_ticks>period*2){o->reason=NA_TIME;return;}
    if(n<3 || (n==3 && !(s[0].flags&NA_SECONDARY)))return;
    float range=maximum-minimum;
    if(range<maxf(8,minimum*0.002f)){o->reason=NA_NOISE;return;}
    /* 判向只用空间差分的稳健趋势；不能用越过端点的二次拟合导数替代观察到的方向。 */
    float d1=(float)s[n-2].cv-s[n-3].cv,d2=(float)s[n-1].cv-s[n-2].cv;
    float d0=n>3?(float)s[n-3].cv-s[n-4].cv:d1;
    float signal_floor=maxf(2,minimum*.005f);
    int up=d0>signal_floor && d1>signal_floor && d2>signal_floor;
    int down=d0<-signal_floor && d1<-signal_floor && d2<-signal_floor;
    if(s[0].flags&NA_SECONDARY){
        float sd1=(float)s[n-2].secondary_cv-s[n-3].secondary_cv;
        float sd2=(float)s[n-1].secondary_cv-s[n-2].secondary_cv;
        float sd0=n>3?(float)s[n-3].secondary_cv-s[n-4].secondary_cv:sd1;
        float sf=maxf(2,minf((float)s[n-3].secondary_cv,(float)s[n-1].secondary_cv)*.005f);
        /* 主指标仍要求逐段显著同向。辅助指标只容许一段微小量化/噪声，
           两段总变化必须超过四倍底噪；超过底噪的反向变化仍否决。 */
        int aux_up=sd1>=-sf && sd2>=-sf && sd1+sd2>4*sf;
        int aux_down=sd1<=sf && sd2<=sf && sd1+sd2<-4*sf;
        up=up && (n==3 || sd0>sf) && ((sd1>sf && sd2>sf) || aux_up);
        down=down && (n==3 || sd0<-sf) && ((sd1<-sf && sd2<-sf) || aux_down);
    }
    float horizon=period+age+(float)t->processing_ticks+(float)t->command_ticks;
    u32 legs=n==3?2:3;float gradients[3];
    for(u32 i=0;i<legs;i++){
        u32 j=n-legs+i;gradients[i]=((float)s[j].cv-s[j-1].cv)/absf((float)s[j].position-s[j-1].position);
    }
    float slope=median(gradients,legs);
    if(up || down){
        float evidence_span=absf((float)s[n-1].position-s[n-3].position);
        o->direction_metric=direction*slope*evidence_span/minimum;
        o->direction_valid=finitef(o->direction_metric);
    }
    if(t->calibrated!=1 || !finitef(t->braking_per_tick2) || t->braking_per_tick2<=0 ||
       (t->command_in_flight && (!finitef(t->commanded_velocity_bound) || t->commanded_velocity_bound<=0))){o->reason=NA_UNCALIBRATED;return;}
    if(fastest>slowest*1.5f && !t->command_in_flight){o->reason=NA_MOTION;return;}
    /* 新阶段命令尚未执行时，不能继续按上一阶段低速来估计提前量。 */
    float prediction_speed=maxf(speed,fastest);
    if(t->command_in_flight)prediction_speed=maxf(prediction_speed,t->commanded_velocity_bound);
    o->prediction_velocity_per_tick=prediction_speed;
    float lead=prediction_speed*horizon,braking=t->braking_per_tick2;o->lead_distance=lead;
    o->required_distance=lead+prediction_speed*prediction_speed/(2*braking)+step;
    if(o->direction_valid && up && slope>0){
        /* 20%相对变化距离是采样密度候选预算，非实测峰宽或可靠焦点。 */
        float sampling_room=minimum*.2f/slope;
        o->safe_speed_ratio=limited_ratio(prediction_speed,horizon,braking,sampling_room);
        o->prediction_valid=finitef(o->safe_speed_ratio) && finitef(o->required_distance);
        o->prediction_kind=1;o->reason=NA_OK;
    }
    for(u32 i=0;i<n;i++)y[i]=((float)s[i].cv-minimum)/range;
    if(!quadratic(x,y,n,b,&noise) || noise>.06f){if(!o->prediction_valid)o->reason=NA_NOISE;return;}
    o->residual=noise*range;
    if(n<5 || b[2]>=-0.02f || b[1]<=0){if(!o->prediction_valid)o->reason=NA_CURVATURE;return;}
    float distance=-b[1]*span/(2*b[2]);
    if(!finitef(distance) || distance<=0 || distance>span*2){if(!o->prediction_valid)o->reason=NA_CURVATURE;return;}
    o->peak_position=s[n-1].position+direction*distance;o->peak_distance=distance;
    if(o->peak_position<-32768 || o->peak_position>32767 || distance<=prediction_speed*age){if(!o->prediction_valid)o->reason=NA_BEHIND;return;}
    /* d = v*T + v²/(2a)，另留一个已测位移步长供原厂继续采样。 */
    o->safe_speed_ratio=minf(o->safe_speed_ratio,limited_ratio(prediction_speed,horizon,braking,distance-step));
    o->prediction_valid=finitef(o->required_distance) && finitef(o->safe_speed_ratio);
    o->prediction_kind=2;o->reason=NA_OK;
}
