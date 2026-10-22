#ifndef HEIF_GRID_LAYOUT_H
#define HEIF_GRID_LAYOUT_H
typedef struct {unsigned columns,rows,tile_width,tile_height;} HeifGridLayout;
/* 原厂 4.2.0 HEIF stream 描述符最多容纳 15 个 tile；不让新文件超过该容量。 */
static int heif_grid_layout(unsigned width,unsigned height,HeifGridLayout *g){
 if(!g||width<2||height<2||(width&1)||(height&1)||width>32768||height>32768)return 0;
 unsigned columns=(width+4799)/4800,rows=(height+5999)/6000;
 if(columns*rows>15)return 0;
 unsigned tw=((width+columns-1)/columns+63)&~63u;
 unsigned th=((height+rows-1)/rows+63)&~63u;
 if((unsigned long long)tw*th>35651584)return 0;
 *g=(HeifGridLayout){columns,rows,tw,th};return 1;
}
#endif
