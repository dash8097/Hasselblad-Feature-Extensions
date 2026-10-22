"""固定4.2.0原QQuickImage构造与完成ABI；数据URI只有SVG几何。"""
import struct
def generate(b,resources):
 symbols={s.name:s for s in b.symbols}
 names={'construct':'_ZN11QQuickImageC1EP10QQuickItem','complete':'_ZN15QQuickImageBase17componentCompleteEv','set_context':'_ZN10QQmlEngine19setContextForObjectEP7QObjectP11QQmlContext'}
 functions={k:symbols[n]['st_value'] for k,n in names.items()}
 assert functions=={'construct':0xf094e0,'complete':0xf0d1e8,'set_context':0x6a8d38}
 address=symbols['_ZN9QtPrivate25QMetaTypeInterfaceWrapperI11QQuickImageE8metaTypeE']['st_value']
 revision,align,size=struct.unpack('<HHI',b.read(address,8));assert revision==0 and align==8 and 16<=size<=4096
 out=['#pragma once','#include "native_animation_view.hpp"','static const UiCode animation_image_functions[]={']
 out+=['{0x%xUL,{%s}},'%(a,','.join('0x%02x'%v for v in b.read(a,16))) for a in functions.values()]
 out+=['};','static x2dui::AnimationImageProfile animation_image_profile(uintptr_t base){x2dui::AnimationImageProfile p{};p.size=%d;'%size]
 out+=['p.%s=reinterpret_cast<decltype(p.%s)>(base+0x%xUL);'%(k,k,a) for k,a in functions.items()]
 out+=['return p;}']
 svg={n:symbols[n]['st_value'] for n in ('_Z27qt_static_plugin_QSvgPluginv','_ZN13QSvgIOHandler4readEP6QImage','_ZN12QSvgRenderer4loadERK10QByteArray')}
 assert all(svg.values())
 return ('\n'.join(out)+'\n').encode(),dict(functions=functions,image_size=size,original_static_svg_symbols=svg,data_uri_svg=True,contains_business_script=False,camera_execution_verified=False)
