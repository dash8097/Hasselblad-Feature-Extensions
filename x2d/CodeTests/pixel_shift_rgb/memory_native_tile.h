#ifndef MEMORY_NATIVE_TILE_H
#define MEMORY_NATIVE_TILE_H
/* 只在原尺寸窗口（含上/下边界的 RGGB 相位复制）所需的行已写齐时显影。
 * 文件是本任务原本就要发布的合成 RAW，不是额外中间图或六张源原片。 */
static inline unsigned memory_native_required_rows(unsigned first){
 if(first>=17498)return 0;
 unsigned end=first+8842-96;
 return end>17498?17498:end;
}
static inline int memory_native_window_ready(unsigned written,unsigned first){
 unsigned needed=memory_native_required_rows(first);
 return needed&&written<=17498&&written>=needed;
}
static inline int memory_native_header(uint8_t h[F3_HEADER]){
 uint32_t first=f3_u32(h+4),raw;
 if(memcmp(h,"II*\0",4)||!f3_scalar(h,F3_HEADER,first,330,&raw))return 0;
 const unsigned tags[]={256,257,258,259,262,277,273,279};
 const uint32_t values[]={23326,17498,16,1,32803,1,16384,816316696};
 for(unsigned i=0;i<8;i++)if(!f3_is(h,F3_HEADER,raw,tags[i],values[i]))return 0;
 return 1;
}
#endif
