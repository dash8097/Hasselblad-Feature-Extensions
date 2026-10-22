/* 离线夹具：逐行生成/完整解码，避免为十六亿图像分配全幅像素缓冲。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include "jpeglib.h"
struct errors {struct jpeg_error_mgr base;jmp_buf stop;};
static void failed(j_common_ptr c){struct errors*e=(struct errors*)c->err;longjmp(e->stop,1);}
static void warn(j_common_ptr c,int level){if(level<0)failed(c);}
int main(int argc,char**argv){
 if(argc!=6)return 2;
 unsigned width=(unsigned)strtoul(argv[3],0,10),height=(unsigned)strtoul(argv[4],0,10);
 if(!width||width>65535||!height||height>65535)return 2;
 struct errors e;memset(&e,0,sizeof e);jpeg_std_error(&e.base);e.base.error_exit=failed;e.base.emit_message=warn;
 if(setjmp(e.stop))return 3;
 unsigned char*row=malloc((size_t)width*3);if(!row)return 4;
 if(!strcmp(argv[1],"encode")){
  FILE*f=fopen(argv[2],"wbx");if(!f)return 5;
  struct jpeg_compress_struct c;memset(&c,0,sizeof c);c.err=&e.base;jpeg_create_compress(&c);jpeg_stdio_dest(&c,f);
  c.image_width=width;c.image_height=height;c.input_components=3;c.in_color_space=JCS_RGB;
  jpeg_set_defaults(&c);c.comp_info[0].h_samp_factor=2;c.comp_info[0].v_samp_factor=1;
  jpeg_set_quality(&c,atoi(argv[5]),TRUE);jpeg_start_compress(&c,TRUE);
  for(unsigned x=0;x<width;x++){row[x*3]=(unsigned char)(x/97);row[x*3+1]=97;row[x*3+2]=173;}
  while(c.next_scanline<c.image_height){JSAMPROW p=row;if(jpeg_write_scanlines(&c,&p,1)!=1)return 6;}
  jpeg_finish_compress(&c);jpeg_destroy_compress(&c);if(fclose(f))return 7;
 }else if(!strcmp(argv[1],"decode")){
  FILE*f=fopen(argv[2],"rb");if(!f)return 5;
  struct jpeg_decompress_struct c;memset(&c,0,sizeof c);c.err=&e.base;jpeg_create_decompress(&c);jpeg_stdio_src(&c,f);
  if(jpeg_read_header(&c,TRUE)!=JPEG_HEADER_OK||c.image_width!=width||c.image_height!=height)return 8;
  c.out_color_space=JCS_RGB;if(!jpeg_start_decompress(&c)||c.output_components!=3||c.output_width!=width||c.output_height!=height)return 9;
  while(c.output_scanline<c.output_height){JSAMPROW p=row;if(jpeg_read_scanlines(&c,&p,1)!=1)return 10;}
  if(!jpeg_finish_decompress(&c))return 11;jpeg_destroy_decompress(&c);fclose(f);
 }else return 2;
 free(row);puts("JPEG_STREAM_COMPLETE");return 0;
}
