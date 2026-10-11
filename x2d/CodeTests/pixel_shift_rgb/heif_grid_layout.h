/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hasselblad Feature Extensions contributors
 */
#ifndef HEIF_GRID_LAYOUT_H
#define HEIF_GRID_LAYOUT_H
typedef struct {unsigned columns,rows,tile_width,tile_height;} HeifGridLayout;
/* 项目网格布局：容量与单块像素上限由调用方提供。 */
static int heif_grid_layout(unsigned width,unsigned height,HeifGridLayout *g,unsigned max_tiles,unsigned max_tile_pixels){
 if(!g||width<2||height<2||(width&1)||(height&1)||width>32768||height>32768)return 0;
 unsigned columns=(width+4799)/4800,rows=(height+5999)/6000;
 if(!max_tiles||columns*rows>max_tiles)return 0;
 unsigned tw=((width+columns-1)/columns+63)&~63u;
 unsigned th=((height+rows-1)/rows+63)&~63u;
 if((unsigned long long)tw*th>max_tile_pixels)return 0;
 *g=(HeifGridLayout){columns,rows,tw,th};return 1;
}
#endif
