"""固定4.2.0原厂右锚getter；不生成QML/JS。"""
import hashlib
def generate(b,resources):
 symbols={s.name:s for s in b.symbols};s=symbols['_ZNK13QQuickAnchors5rightEv'];address=s['st_value'];assert address==0xef5cd0 and s['st_size']>0
 qml=resources[':/app/qml/mainmenu/delegates/TextDelegate.qml']
 assert qml.count(b'right: parent.right')==1 and qml.count(b'objectName: "TextDelegate_root"')==1 and b'ValueFormatter.getDisplayValue(root.model.itemName, root.model.propValue)' in qml
 header=['#pragma once','#include "about_attachment.hpp"','static const UiCode about_functions[]={{0x%xUL,{%s}}};'%(address,','.join('0x%02x'%v for v in b.read(address,16))),'static x2dui::AboutAttachmentProfile about_profile(uintptr_t base){x2dui::AboutAttachmentProfile p{};p.right=reinterpret_cast<decltype(p.right)>(base+0x%xUL);p.rightLine=2;return p;}'%address]
 return ('\n'.join(header)+'\n').encode(),dict(right_getter=address,right_anchor_enum=2,text_delegate_sha256=hashlib.sha256(qml).hexdigest(),uses_existing_factory_rows=True,changes_identity=False)
