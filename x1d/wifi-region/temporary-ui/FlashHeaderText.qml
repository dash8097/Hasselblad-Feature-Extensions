import QtQuick 2.5

Item {
    id:root
    property alias text:label.text
    property alias font:label.font
    property alias horizontalAlignment:label.horizontalAlignment
    property alias wrapMode:label.wrapMode
    TextMetrics {id:ink;font:label.font;text:label.text}
    Text {
        id:label;width:parent.width;color:"white"
        // Center the actual glyph bounds, not the font's ascent/descent box.
        y:Math.round((root.height-ink.tightBoundingRect.height)/2-ink.tightBoundingRect.y-baselineOffset)
        verticalAlignment:Text.AlignTop
    }
}
