/* 4.2.0 固定 ABI。仅排队选眼参数，不调用 AF 开始、镜头移动或快门接口。 */
#define EYE_COMMAND 0x40020000
#define EYE_OBSERVE (EYE_COMMAND+5)
/* 协议2的 +5 仅回读，绝不向原厂队列再次提交选择。时间均为 CLOCK_MONOTONIC。 */
static int eye_last_command=-1,eye_last_requested=-1,eye_last_result=-1,eye_last_handle=-1;
static I64 eye_command_ms=-1;
#ifndef FACE_BACKEND_TEST
extern int clock_gettime(int,void*);
static I64 eye_now(void) {struct {I64 sec,nsec;} t;if(clock_gettime(1,&t)||t.sec<0||t.sec>9223372036854774LL||t.nsec<0||t.nsec>=1000000000)return -1;return t.sec*1000+t.nsec/1000000;}
#else
static I64 eye_test_now;
static I64 eye_now(void){return eye_test_now;}
static int eye_test_report[5];static I64 eye_test_times[2];
#endif
static int eye_saved=-1,eye_saved_handle=-1;
static int (*eye_handle)(void);
static int (*eye_valid_handle)(int);
static int (*eye_get)(int);
static int (*eye_queue)(int,const unsigned*);
static U eye_aaa,eye_rcam;
#ifndef FACE_BACKEND_TEST
static int eye_bases(struct DlInfo*d,U n,void*x) {
    (void)n;(void)x;
    if(d->name && equal(d->name,"/system/lib64/libaaa.so"))eye_aaa=d->addr;
    if(d->name && equal(d->name,"/system/lib64/librcam.so"))eye_rcam=d->addr;
    return 0;
}
static int eye_resolve(void) {
    dl_iterate_phdr(eye_bases,0);
    if(!eye_aaa||!eye_rcam)return 0;
    if(!public_code_digest((void*)(eye_aaa+0x33220),4,"3f07c89f3e39893d4731c5865d01df58e36cb6a0787414cf6d4da7d46a6f08b8") ||
       !public_code_digest((void*)(eye_aaa+0x35480),4,"d4aeb0c5c16e604bcdad91c5188e82c0a328b80cab7266ed3d6babf6da253a72") ||
       !public_code_digest((void*)(eye_aaa+0xc7910),4,"826a882b0c59c1e16abb097a7413227f93a720b222c30c3f31f1e3e35b9d5599") ||
       !public_code_digest((void*)(eye_rcam+0xb9ff8),4,"ef9193e88447246e122a111a61ba8e3c3b767324bb563a5603d02d069fb05126"))return 0;
    eye_handle=(void*)(eye_rcam+0xb9ff8);
    eye_valid_handle=(void*)(eye_aaa+0xc7910);
    eye_get=(void*)(eye_aaa+0x35480);
    eye_queue=(void*)(eye_aaa+0x33220);
    return 1;
}
static void eye_report(int requested,int actual,int result) {
    char s[320];int n=snprintf(s,sizeof(s),"{\"protocol\":2,\"command\":%d,\"requested\":%d,\"actual\":%d,\"result\":%d,\"pid\":%d,\"command_ms\":%lld,\"sampled_ms\":%lld}\n",eye_last_command,requested,actual,result,getpid(),(long long)eye_command_ms,(long long)eye_now());
    if(n<1||(U)n>=sizeof(s))return;
    int f=open("/tmp/x2d-eye-af-state.json",0xA0241,0644);
    if(f>=0){write(f,s,(U)n);close(f);}
}
#else
static int eye_resolve(void){return eye_handle&&eye_valid_handle&&eye_get&&eye_queue;}
static void eye_report(int requested,int actual,int result){eye_test_report[0]=eye_last_command;eye_test_report[1]=requested;eye_test_report[2]=actual;eye_test_report[3]=result;eye_test_report[4]++;eye_test_times[0]=eye_command_ms;eye_test_times[1]=eye_now();}
#endif
static int eye_observe(void) {
    int actual=-1,result=-1;
    if(eye_resolve()) {
        int h=eye_handle();
        if(h>=0&&h<32&&h==eye_last_handle&&eye_valid_handle(h)==0) {
#ifndef FACE_BACKEND_TEST
            if(!((U*)(eye_aaa+0xa290c8))[h]||!((U*)(eye_aaa+0xa291c8))[h])goto done;
#endif
            actual=eye_get(h);if(actual>=0&&actual<=3)result=eye_last_result;else actual=-1;
        }
    }
#ifndef FACE_BACKEND_TEST
done:
#endif
    eye_report(eye_last_requested,actual,result);return 1;
}
static int eye_command(int command) {
    if(command==EYE_OBSERVE)return eye_observe();
    if(command<EYE_COMMAND || command>EYE_COMMAND+4)return 0;
    int desired=command-EYE_COMMAND,actual=-1,result=-1;
    eye_last_command=desired;eye_last_handle=-1;eye_command_ms=eye_now();
    if(!eye_resolve())goto done;
#ifndef FACE_BACKEND_TEST
    eye_install_age_limit();
    status(1);
#endif
    int h=eye_handle();
    if(h<0||h>=32||eye_valid_handle(h)!=0)goto done;
    eye_last_handle=h;
#ifndef FACE_BACKEND_TEST
    if(!((U*)(eye_aaa+0xa290c8))[h] || !((U*)(eye_aaa+0xa291c8))[h])goto done;
#endif
    actual=eye_get(h);
    if(actual<0||actual>3)goto done;
    if(desired==4) {
        if(eye_saved<0 || eye_saved_handle!=h){result=0;goto done;}
        desired=eye_saved;
    } else if(eye_saved_handle!=h) {eye_saved=actual;eye_saved_handle=h;}
    if(actual==desired){result=0;goto done;}
    /* 原厂 af_cmd_eye_select_s: opcode, 调度类别, size, select。 */
    unsigned cmd[4]={0x203,1,16,(unsigned)desired};
    result=eye_queue(h,cmd);
    actual=eye_get(h); /* 入队成功不等于已经消费，回读结果单独报告。 */
done:
    eye_last_requested=desired;eye_last_result=result;
    eye_report(desired,actual,result);
    return 1;
}
