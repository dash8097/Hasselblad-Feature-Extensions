"""Separate AF frame movement from repainting its unchanged Canvas texture."""
def apply_focus_indicator_motion(text):
    block='''    Connections {
        target: GlobalStateInfo.focusDelivery
        onDisplayedChanged: af_symbol.requestPaint()
    }
'''
    assert text.count(block)==1
    text=text.replace(block,'')
    start=text.index('        onFocus_pointChanged: {')
    end=text.index('        onFocus_sizeChanged:',start)
    text=text[:start]+text[end:]
    # During dragging, an acknowledgement must not alternate idle/result artwork.
    text=text.replace('GlobalStateInfo.focusDelivery.pending','(GlobalStateInfo.focusDelivery.pending || GlobalStateInfo.focusDelivery.dragging)')
    return text
