/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hasselblad Feature Extensions contributors
 */
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include "heif_grid_layout.h"
int main(void){
 HeifGridLayout g;
 assert(heif_grid_layout(23326,17498,&g,15,35651584));
 assert(g.columns==5&&g.rows==3&&g.tile_width==4672&&g.tile_height==5888);
 assert((unsigned long long)g.columns*g.tile_width>=23326);
 assert((unsigned long long)g.rows*g.tile_height>=17498);
 assert(heif_grid_layout(64,48,&g,15,35651584)&&g.columns==1&&g.rows==1&&g.tile_width==64&&g.tile_height==64);
 assert(!heif_grid_layout(65534,17498,&g,15,35651584));assert(!heif_grid_layout(23327,17498,&g,15,35651584));
 assert(!heif_grid_layout(32768,32768,&g,15,35651584));
 assert(!heif_grid_layout(23326,17498,&g,14,35651584));
 assert(!heif_grid_layout(64,48,&g,1,4095));
 assert(heif_grid_layout(64,48,&g,1,4096));
 puts("HEIF_408MP_GRID_15_TILE_CAPACITY_AND_EDGE_PADDING_CHECKED");return 0;
}
