"""固定原厂Qt 6.4.1 list<QQuickItem>元类型；仅用于Popover.content。"""
import struct
def generate(b):
 sy={s.name:s for s in b.symbols};name='_ZN9QtPrivate25QMetaTypeInterfaceWrapperI16QQmlListPropertyI10QQuickItemEE8metaTypeE'
 s=sy[name];a=s['st_value'];assert a and s['st_size'];revision,align,size=struct.unpack('<HHI',b.read(a,8));assert align==8 and size==64
 out=['#pragma once','#include "face_popup_view.hpp"','static const uintptr_t face_popup_list_type=0x%xUL;'%a,'static x2dui::FacePopupProfile face_popup_profile(uintptr_t base){return {reinterpret_cast<void*>(base+face_popup_list_type)};}']
 return ('\n'.join(out)+'\n').encode(),dict(firmware='X2D 100C 4.2.0',metatype=a,alignment=align,size=size,only_property='content',callbacks='owned original QObject list, count must be zero before append')
