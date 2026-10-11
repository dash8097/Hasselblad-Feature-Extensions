/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hasselblad Feature Extensions contributors
 */
/* 四亿 JPEG 区域解码离线/独立探针。按扫描行解码，仅保存 ROI；不连接相机服务。 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <setjmp.h>
#include <jpeglib.h>

struct failure { struct jpeg_error_mgr pub; jmp_buf jump; };
struct work {
    struct jpeg_decompress_struct jpeg;
    struct failure failure;
    FILE *input, *output;
    unsigned char *row;
    int created, owned;
};
static void failed(j_common_ptr p) {
    struct failure *f=(struct failure *)p->err;
    /* 不输出输入文件路径或图像元数据。 */
    fprintf(stderr,"JPEG_DECODE_ERROR code=%d\n",p->err->msg_code);
    longjmp(f->jump,1);
}
static int number(const char *s, unsigned *value) {
    char *end; unsigned long v;
    if(!s[0] || s[0]=='-' || s[0]=='+')return 0;
    errno=0;v=strtoul(s,&end,10);
    if(errno || *end || v>50000)return 0;
    *value=(unsigned)v;return 1;
}
int main(int argc,char **argv) {
    unsigned x,y,w,h;
    if(argc!=7 || !number(argv[3],&x) || !number(argv[4],&y) ||
       !number(argv[5],&w) || !number(argv[6],&h) || !w || !h ||
       w>3840 || h>3840 || x>=23326 || y>=17498 || w>23326-x || h>17498-y)return 2;
    /* 堆对象在 libjpeg longjmp 后仍保持可清理状态。 */
    struct work *a=calloc(1,sizeof(*a));if(!a)return 3;
    int result=1;
    a->jpeg.err=jpeg_std_error(&a->failure.pub);
    a->failure.pub.error_exit=failed;
    if(setjmp(a->failure.jump))goto cleanup;
    a->input=fopen(argv[1],"rb");if(!a->input)goto cleanup;
    jpeg_create_decompress(&a->jpeg);a->created=1;
    jpeg_stdio_src(&a->jpeg,a->input);
    if(jpeg_read_header(&a->jpeg,TRUE)!=JPEG_HEADER_OK)goto cleanup;
    /* 渐进式/多扫描需要整幅系数缓存，明确拒绝，不能靠内存上限配置猜测。 */
    if(a->jpeg.image_width!=23326 || a->jpeg.image_height!=17498 ||
       a->jpeg.progressive_mode || jpeg_has_multiple_scans(&a->jpeg) ||
       a->jpeg.data_precision!=8 || a->jpeg.num_components!=3 ||
       a->jpeg.jpeg_color_space!=JCS_YCbCr)goto cleanup;
    a->jpeg.out_color_space=JCS_RGB;
    a->jpeg.dct_method=JDCT_ISLOW;
    a->jpeg.do_fancy_upsampling=TRUE;
    if(!jpeg_start_decompress(&a->jpeg) || a->jpeg.output_components!=3)goto cleanup;
    size_t row_bytes=(size_t)a->jpeg.output_width*3;
    a->row=malloc(row_bytes);if(!a->row)goto cleanup;
    /* exclusive 防止覆盖；调用方只可使用本轮专有缓存路径。 */
    a->output=fopen(argv[2],"wbx");if(!a->output)goto cleanup;
    a->owned=1;
    if(fprintf(a->output,"P6\n%u %u\n255\n",w,h)<0)goto cleanup;
    while(a->jpeg.output_scanline<y+h) {
        unsigned line=a->jpeg.output_scanline;
        JSAMPROW row=a->row;
        if(jpeg_read_scanlines(&a->jpeg,&row,1)!=1)goto cleanup;
        if(line>=y && fwrite(a->row+(size_t)x*3,3,w,a->output)!=w)goto cleanup;
    }
    if(a->jpeg.output_scanline==a->jpeg.output_height) {
        if(!jpeg_finish_decompress(&a->jpeg))goto cleanup;
    } else jpeg_abort_decompress(&a->jpeg);
    /* 截断输入由库产生告警，即使填充后可解码，也不能当成合格图块。 */
    if(a->failure.pub.num_warnings)goto cleanup;
    if(fclose(a->output)){a->output=NULL;goto cleanup;}
    a->output=NULL;result=0;
    printf("ROI_DECODE_OK width=%u height=%u row_bytes=%zu rgb_bytes=%zu\n",w,h,row_bytes,(size_t)w*h*3);
cleanup:
    if(a->output)fclose(a->output);
    if(a->created)jpeg_destroy_decompress(&a->jpeg);
    if(a->input)fclose(a->input);
    free(a->row);
    if(result && a->owned)remove(argv[2]);
    free(a);return result;
}
