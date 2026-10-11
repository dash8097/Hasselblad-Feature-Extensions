import QtQuick

// 固定 4.2.0 SettingsGeneric 契约；不改原厂设置模型或原声音属性。
Item {
    id: root
    visible: false
    property var searchRoot: null
    property var preferences: null
    property var targetList: null
    property var targetPage: null
    property Component previousFooter: null
    property bool active: true
    function findPage(item) {
        if (!item) return null
        if (item.objectName.indexOf("SettingsGeneric_root") === 0 && item.menuName === "exposureMenu") return item
        for (var i=0; i<item.children.length; ++i) {
            var found=findPage(item.children[i]); if(found) return found
        }
        return null
    }
    function findList(page) {
        if (!page) return null
        for(var i=0;i<page.children.length;++i)
            if(page.children[i].objectName === "SettingsGeneric_list") return page.children[i]
        return null
    }
    function detach() {
        if(targetList && targetList.footer === footer) targetList.footer=previousFooter
        targetList=null; targetPage=null; previousFooter=null
    }
    function refresh() {
        var page=active ? findPage(searchRoot) : null
        var list=findList(page)
        if(list === targetList) return
        detach()
        // 原厂 4.2.0 此列表无 footer；未知扩展已有页尾时不覆盖。
        if(!list || list.footer) return
        targetPage=page; targetList=list; previousFooter=list.footer; list.footer=footer
    }
    Component {
        id: footer
        X2dExposureOption {
            preferences: root.preferences
            width: root.targetList ? root.targetList.width : 0
            rowHeight: root.targetPage ? root.targetPage.heightForItem : 88
            textSize: rowHeight * .32
        }
    }
    Timer { interval: 200; repeat: true; running: root.active && root.searchRoot !== null; onTriggered: root.refresh() }
    onActiveChanged: refresh()
    Component.onDestruction: detach()
}
