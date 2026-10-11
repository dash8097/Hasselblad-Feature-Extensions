#include "../native/godox_power_wave.h"
#include <stdio.h>
#include <string.h>

#define CHECK(value) do { if (!(value)) { fprintf(stderr,"line %d\n",__LINE__); return 1; } } while (0)

int main(int argc,char **argv) {
    uint32_t lut[1024],hash=0;
    uint8_t frame[12],saved_frame[12];
    HblGodoxPowerRun runs[96],saved_runs[96];
    if (argc!=2) return 2;
    FILE *input=fopen(argv[1],"rb");
    if (!input) return 3;
    const size_t words=fread(lut,sizeof(uint32_t),1024,input);
    const int extra=fgetc(input);
    fclose(input);
    CHECK(words==1024 && extra==EOF);
    memset(frame,0x5a,sizeof(frame));
    memcpy(saved_frame,frame,sizeof(frame));
    memset(runs,0x5a,sizeof(runs));
    memcpy(saved_runs,runs,sizeof(runs));
    CHECK(!hbl_godox_power_frame_encode(NULL,0,0));
    CHECK(!hbl_godox_power_frame_encode(frame,5,0));
    CHECK(!hbl_godox_power_frame_encode(frame,0,81));
    CHECK(!memcmp(frame,saved_frame,sizeof(frame)));
    CHECK(!hbl_godox_power_runs_encode(NULL,0,0));
    CHECK(!hbl_godox_power_runs_encode(runs,5,0));
    CHECK(!hbl_godox_power_runs_encode(runs,0,81));
    CHECK(!memcmp(runs,saved_runs,sizeof(runs)));
    CHECK(!hbl_godox_power_wave_hash(NULL,runs,lut));
    CHECK(!hbl_godox_power_wave_hash(&hash,NULL,lut));
    CHECK(!hbl_godox_power_wave_hash(&hash,runs,NULL));
    CHECK(hbl_godox_power_runs_encode(runs,3,47));
    runs[10].reserved=1; hash=0x1234;
    CHECK(!hbl_godox_power_wave_hash(&hash,runs,lut) && hash==0x1234);
    runs[10].reserved=0; ++runs[10].samples;
    CHECK(!hbl_godox_power_wave_hash(&hash,runs,lut) && hash==0x1234);
    --runs[10].samples; runs[10].phase_step=0;
    CHECK(!hbl_godox_power_wave_hash(&hash,runs,lut) && hash==0x1234);
    for (unsigned group=0;group<5;++group) {
        for (unsigned value=0;value<=80;++value) {
            CHECK(hbl_godox_power_frame_encode(frame,group,value));
            CHECK(hbl_godox_power_runs_encode(runs,group,value));
            CHECK(frame[8]==0xa9 && frame[9]==0x0a+group && frame[10]==0xbc && frame[11]==value);
            CHECK(hbl_godox_power_wave_hash(&hash,runs,lut));
            for (unsigned i=0;i<12;++i) printf("%02x",frame[i]);
            printf(" %08x\n",hash);
        }
    }
    return 0;
}
