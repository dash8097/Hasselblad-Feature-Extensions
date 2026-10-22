/* X2D 100C 4.2.0 缓冲几何候选。单位显式区分；不创建设备或提交请求。
 * 描述字段来自 factory-native-descriptor-geometry.json。
 * 四亿仅通过离线算术校验，空间标定与硬件运行仍未验证。 */
#ifndef FACTORY_BUFFER_GEOMETRY_H
#define FACTORY_BUFFER_GEOMETRY_H
#include <stdint.h>
#ifndef FACTORY_GEOMETRY_MAX_SIDE
#define FACTORY_GEOMETRY_MAX_SIDE 32768u
#endif
typedef struct {
 uint32_t width,height,offset,content_size;
} FactoryPlaneGeometry;
typedef struct {
 uint32_t width,height,stride_pixels,row_bytes,format,source_format;
 uint32_t crop_x,crop_y,crop_w,crop_h,plane_count;
 uint32_t content_bytes,allocation_bytes;
 FactoryPlaneGeometry planes[2];
} FactoryBufferGeometry;

/* 容量按已观察到的 4096 字节页对齐计算；实际分配仍须回读确认。
 * 使用 64 位中间量；失败时不修改调用方输出。 */
static int factory_buffer_geometry(FactoryBufferGeometry *out,
 uint32_t width,uint32_t height,uint32_t format) {
 if(!out||!width||!height||(width&1)||(height&1)||width>FACTORY_GEOMETRY_MAX_SIDE||height>FACTORY_GEOMETRY_MAX_SIDE)
  return 0;
 if(format!=1&&format!=6)return 0;
 uint64_t alignment=format==1?128:256;
 uint64_t stride=((uint64_t)width+alignment-1)&~(alignment-1);
 uint64_t row=stride*(format==1?2:1),plane=row*height;
 uint64_t content=plane*(format==1?1:2),allocation=(content+4095)&~UINT64_C(4095);
 if(allocation>UINT32_MAX)return 0;
 FactoryBufferGeometry g={0};
 g.width=width;g.height=height;g.stride_pixels=(uint32_t)stride;
 g.row_bytes=(uint32_t)row;g.format=format;g.source_format=255;
 g.crop_w=width;g.crop_h=height;g.plane_count=format==1?1:2;
 g.content_bytes=(uint32_t)content;g.allocation_bytes=(uint32_t)allocation;
 for(uint32_t i=0;i<g.plane_count;i++) {
  g.planes[i].width=width;g.planes[i].height=height;
  g.planes[i].offset=i*(uint32_t)plane;g.planes[i].content_size=(uint32_t)plane;
 }
 *out=g;return 1;
}
/* 由描述符到实际分配的边界检查；不证明设备已导入或完成显影。 */
static int factory_buffer_fits(const FactoryBufferGeometry *g,uint64_t allocated) {
 if(!g)return 0;
 FactoryBufferGeometry expected;
 if(!factory_buffer_geometry(&expected,g->width,g->height,g->format))return 0;
 if(g->stride_pixels!=expected.stride_pixels||g->row_bytes!=expected.row_bytes||
    g->source_format!=255||g->crop_x||g->crop_y||g->crop_w!=g->width||
    g->crop_h!=g->height||g->plane_count!=expected.plane_count||
    g->content_bytes!=expected.content_bytes||g->allocation_bytes!=expected.allocation_bytes||
    allocated<g->allocation_bytes||allocated>UINT32_MAX)return 0;
 for(unsigned i=0;i<2;i++) {
  const FactoryPlaneGeometry *a=&g->planes[i],*b=&expected.planes[i];
  if(a->width!=b->width||a->height!=b->height||a->offset!=b->offset||
     a->content_size!=b->content_size)return 0;
 }
 return 1;
}
#endif
