#include "quarter_software_jpeg.h"
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <jpeglib.h>
#define QN_JPEG_LIMIT (64u*1024*1024)
struct QnJpegError {struct jpeg_error_mgr pub;jmp_buf jump;};
struct QnJpeg {struct jpeg_decompress_struct dec;struct jpeg_compress_struct enc;struct QnJpegError error;struct jpeg_destination_mgr dest;uint8_t *row,*output;int dec_created,enc_created;};
static void error_exit(j_common_ptr c){longjmp(((struct QnJpegError*)c->err)->jump,1);}
static void quiet(j_common_ptr c){(void)c;}
static void destination(j_compress_ptr c){struct QnJpeg *a=c->client_data;c->dest->next_output_byte=a->output;c->dest->free_in_buffer=QN_JPEG_LIMIT;}
static boolean overflow(j_compress_ptr c){error_exit((j_common_ptr)c);return FALSE;}
static void finish_destination(j_compress_ptr c){(void)c;}
int qn_jpeg_half(const uint8_t *input,size_t bytes,uint8_t **output,size_t *length){
 if(!output||!length)return 0;*output=NULL;*length=0;
 if(!input||bytes<4||bytes>128u*1024*1024)return 0;
 struct QnJpeg *a=calloc(1,sizeof *a);if(!a)return 0;int ok=0;
 a->dec.err=jpeg_std_error(&a->error.pub);a->enc.err=&a->error.pub;
 a->error.pub.error_exit=error_exit;a->error.pub.output_message=quiet;
 if(setjmp(a->error.jump))goto done;
 jpeg_create_decompress(&a->dec);a->dec_created=1;jpeg_mem_src(&a->dec,input,(unsigned long)bytes);
 if(jpeg_read_header(&a->dec,TRUE)!=JPEG_HEADER_OK||a->dec.image_width!=11656||a->dec.image_height!=8742||a->dec.data_precision!=8||a->dec.num_components!=3||a->dec.progressive_mode||a->dec.arith_code||jpeg_has_multiple_scans(&a->dec)||a->error.pub.num_warnings)goto done;
 a->dec.scale_num=1;a->dec.scale_denom=2;a->dec.out_color_space=JCS_RGB;a->dec.dct_method=JDCT_ISLOW;
 if(!jpeg_start_decompress(&a->dec)||a->dec.output_width!=5828||a->dec.output_height!=4371||a->dec.output_components!=3)goto done;
 a->row=malloc(5828u*3);a->output=malloc(QN_JPEG_LIMIT);if(!a->row||!a->output)goto done;
 jpeg_create_compress(&a->enc);a->enc_created=1;a->enc.client_data=a;
 a->dest.init_destination=destination;a->dest.empty_output_buffer=overflow;a->dest.term_destination=finish_destination;a->enc.dest=&a->dest;
 a->enc.image_width=5828;a->enc.image_height=4371;a->enc.input_components=3;a->enc.in_color_space=JCS_RGB;
 jpeg_set_defaults(&a->enc);jpeg_set_quality(&a->enc,80,TRUE);a->enc.dct_method=JDCT_ISLOW;jpeg_start_compress(&a->enc,TRUE);
 while(a->dec.output_scanline<a->dec.output_height){JSAMPROW row=a->row;if(jpeg_read_scanlines(&a->dec,&row,1)!=1||jpeg_write_scanlines(&a->enc,&row,1)!=1)goto done;}
 if(!jpeg_finish_decompress(&a->dec))goto done;jpeg_finish_compress(&a->enc);
 if(a->error.pub.num_warnings)goto done;
 *length=QN_JPEG_LIMIT-a->dest.free_in_buffer;*output=a->output;a->output=NULL;ok=1;
 done:
 if(a->enc_created)jpeg_destroy_compress(&a->enc);if(a->dec_created)jpeg_destroy_decompress(&a->dec);
 free(a->row);free(a->output);free(a);return ok;
}
