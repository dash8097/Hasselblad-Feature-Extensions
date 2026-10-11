"""Keep flash subpages alive until the release animation actually completes."""
def apply_flash_release(source):
    def replace(old, new):
        nonlocal source
        assert old in source, old
        source = source.replace(old, new)

    marker = '    function closeDetail() { screen="groups" }'
    replace(marker, '''    function settleFlashReturn(pane, leave, sign) {
        if (flashReturn.running) return
        flashReturn.target=pane
        flashReturn.from=pane.x
        flashReturn.to=flashStyle.pageInset+(leave ? (sign<0 ? -640 : 640) : 0)
        flashReturn.leave=leave
        flashReturn.restart()
    }
    NumberAnimation {
        id:flashReturn;objectName:"FlashReturnAnimation";property:"x"
        duration:200;easing.type:Easing.OutCubic
        property bool leave:false
        property bool resetting:false
        onStopped:{
            if(leave && target && Math.abs(target.x-to)<1){
                resetting=true
                page.requestBack()
                if(target===detailPane)detailPane.x=flashStyle.pageInset
                resetting=false
            }
            detailSwipe.direction=0
        }
    }
''' + marker)
    replace('enabled:!selectionSwipe.drag.active && !selectionBlankSwipe.pressed;',
            'enabled:!flashReturn.running && !selectionSwipe.drag.active && !selectionBlankSwipe.pressed;')
    replace('enabled:!settingsSwipe.drag.active;', 'enabled:!flashReturn.running && !settingsSwipe.drag.active;')
    replace('enabled:!detailSwipe.pressed;', 'enabled:!flashReturn.running && !flashReturn.resetting && !detailSwipe.pressed;')
    replace('if(selectionPane.x>flashStyle.pageInset+640/constants.swipeLengthDividor){page.requestBack();selectionPane.x=flashStyle.pageInset+640}else selectionPane.x=flashStyle.pageInset',
            'page.settleFlashReturn(selectionPane,selectionPane.x>flashStyle.pageInset+640/constants.swipeLengthDividor,1)')
    replace('if(horizontal && selectionPane.x>flashStyle.pageInset+640/constants.swipeLengthDividor){page.requestBack();selectionPane.x=flashStyle.pageInset+640}\n                else selectionPane.x=flashStyle.pageInset',
            'page.settleFlashReturn(selectionPane,horizontal && selectionPane.x>flashStyle.pageInset+640/constants.swipeLengthDividor,1)')
    replace('if(settingsList.x>flashStyle.pageInset+640/constants.swipeLengthDividor){page.requestBack();settingsList.x=flashStyle.pageInset+640}\n                else settingsList.x=flashStyle.pageInset',
            'page.settleFlashReturn(settingsList,settingsList.x>flashStyle.pageInset+640/constants.swipeLengthDividor,1)')
    replace('if(direction===1 && Math.abs(dx)>640/constants.swipeLengthDividor)page.requestBack()',
            'if(direction===1){page.settleFlashReturn(detailPane,Math.abs(dx)>640/constants.swipeLengthDividor,dx);return}')
    return source
