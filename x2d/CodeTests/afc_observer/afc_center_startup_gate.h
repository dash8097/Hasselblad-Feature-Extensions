#ifndef AFC_CENTER_STARTUP_GATE_H
#define AFC_CENTER_STARTUP_GATE_H
/* Only the bounded status header is inspected; history/events are never parsed.
   A previous process's ready file must never authorize a new startup. */
static int afc_center_peer_header(const char *text, unsigned size, int pid) {
    const char ready[]="\"ready\":true", key[]="\"pid\":";
    unsigned ready_count=0,pid_count=0; int found_pid=0;
    if(!text||!size||pid<=0)return 0;
    for(unsigned i=0;i<size;++i){
        if(text[i]=='[')break;
        unsigned j=0;for(;j<sizeof(ready)-1&&i+j<size&&text[i+j]==ready[j];++j){}
        if(j==sizeof(ready)-1&&i+j<size&&(text[i+j]==','||text[i+j]=='}'))++ready_count;
        j=0;for(;j<sizeof(key)-1&&i+j<size&&text[i+j]==key[j];++j){}
        if(j==sizeof(key)-1){
            unsigned k=i+j,n=0,digits=0;
            while(k<size&&text[k]>='0'&&text[k]<='9'){
                if(n>214748364u||(n==214748364u&&text[k]>'7'))return 0;
                n=n*10+(unsigned)(text[k++]-'0');++digits;
            }
            if(!digits||k==size||(text[k]!=','&&text[k]!='}'))return 0;
            ++pid_count;found_pid=(int)n;
        }
    }
    return ready_count==1&&pid_count==1&&found_pid==pid;
}
#endif
