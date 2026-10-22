/* 4.2.0 af_roi_manager_run: 只修改检测帧年龄比较，不修改原厂库文件。 */
static int eye_age_instruction_valid(unsigned word,unsigned next) {
    return public_code_digest(&next,4,"75ffc518df2040f22c260716bd2242838bba85de4601019bfca19730524cd85d") && (public_code_digest(&word,4,"b12650fb5880a860549f00685b91bac05caff673ac5cf75376c59161772c16e7") || public_code_digest(&word,4,"39934de298c1392a70606b9fd57d308aebe0fbfbbe464b53151d20a981853611"));
}
#ifndef FACE_BACKEND_TEST
extern long pwrite(int,const void*,U,long);
static int eye_age_frames;
static int eye_age_error;
static void eye_install_age_limit(void) {
    if(eye_age_frames==10)return;
    eye_age_error=1;
    dl_iterate_phdr(eye_bases,0);
    if(!eye_aaa)return;
    volatile unsigned *at=(volatile unsigned*)(eye_aaa+0xba738);
    unsigned before=at[0],next=at[1],after=(0x71000000u|(10u<<10)|(8u<<5)|31u);
    eye_age_error=2;
    if(!eye_age_instruction_valid(before,next))return;
    if(before!=after) {
        eye_age_error=3;
        int f=open("/proc/self/mem",0x80002);
        if(f<0)return;
        long n=pwrite(f,&after,4,(long)(U)at);
        close(f);
        eye_age_error=4;
        if(n!=4)return;
    }
    if(at[0]==after && at[1]==next){eye_age_frames=10;eye_age_error=0;}
}
#endif
