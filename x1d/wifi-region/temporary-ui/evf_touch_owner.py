"""Keep one EVF touch contact for the duration of a relative drag."""
def apply_touch_owner(text):
    text=text.replace('    property bool focusDragHeld: false','    property int activeTouchId: -1\n    property bool focusDragHeld: false',1)
    text=text.replace('        focusDragHeld = false','        focusDragHeld = false\n        activeTouchId = -1\n        wasInTouchArea = false',1)
    start=text.index('    onPressed: {')
    end=text.index('    function isInTouchArea',start)
    return text[:start]+'''    function acquireTouch(points) {
        if (activeTouchId >= 0) return
        for (var i=0;i<points.length;i++) {
            var p=points[i]
            if (!isInTouchArea(p.x,p.y)) continue
            activeTouchId=p.pointId
            GlobalStateInfo.touchpadZoomActive=inZoomMode
            wasInTouchArea=true
            beginFocusDrag()
            xLastPos=p.x;yLastPos=p.y
            return
        }
    }
    function releaseTouch(points) {
        for (var i=0;i<points.length;i++) {
            if (points[i].pointId !== activeTouchId) continue
            finishFocusDrag()
            GlobalStateInfo.touchpadZoomActive=false
            return
        }
    }
    function updateTouch(points) {
        if (!enabled) return
        if (activeTouchId < 0) {acquireTouch(points);return}
        for (var i=0;i<points.length;i++) {
            var p=points[i]
            if (p.pointId !== activeTouchId) continue
            if (!isInTouchArea(p.x,p.y)) {wasInTouchArea=false;return}
            if (!wasInTouchArea) {
                beginFocusDrag();xLastPos=p.x;yLastPos=p.y
                wasInTouchArea=true;return
            }
            if (inZoomMode && GlobalStateInfo.touchpadZoomActive) updateZoomPosition(p.x,p.y)
            else if (inFocusMode && !GlobalStateInfo.touchpadZoomActive) updateAfIndexRelativePosition(p.x,p.y)
            return
        }
    }
    onPressed: acquireTouch(touchPoints)
    onReleased: releaseTouch(touchPoints)
    onTouchUpdated: updateTouch(touchPoints)

'''+text[end:]
