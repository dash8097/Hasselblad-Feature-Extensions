import QtQuick
import com.hasselblad.constants
import com.hasselblad.proxies
import com.hasselblad.valueformatter
import com.hasselblad.menumodel
import "qrc:/app/qml/mainmenu"

X2dDisplayModeList {
    id:root
    property MainMenuViewModel viewModel:null
    readonly property bool preventSwipe:true
    property var stockValues:[]
    property var choiceList:null
    function findList(item) {
        if(item.objectName==="ListSelectorSettings_list") return item
        var children=item.children || []
        for(var i=0;i<children.length;++i) { var hit=findList(children[i]); if(hit) return hit }
        return null
    }
    function setup() {
        var values=ValueFormatter.getVariableListModel(propName)
        var zeroName=ValueFormatter.getUntranslatedDisplayValue(propName,0)
        var filtered=[]
        for(var i=0;i<values.length;++i) if(values[i]!==zeroName && values[i]!=="关闭") filtered.push(values[i])
        stockValues=filtered
        choiceList=findList(root)
        if(!choiceList || stockValues.length===0) { close(); return }
        choiceList.model=stockValues.concat(["关闭"])
        choiceList.currentIndex=Number(propValue)===0 ? stockValues.length :
            stockValues.indexOf(ValueFormatter.getUntranslatedDisplayValue(propName,propValue))
        choiceList.visible=true
    }
    function setSelected() {
        if(!choiceList) return
        var ix=choiceList.currentIndex
        if(ix<0 || ix>stockValues.length) return
        if(ix===stockValues.length) System.screen_idle_timeout=0
        else ValueFormatter.setSettingValue(proxy,propName,stockValues[ix])
        valueSaved()
    }
    interactive:!viewModel.screenEVF
    popupAnchor {
        leftMargin:320*Constants.scaleFactorX
        topMargin:131.2*Constants.scaleFactorY
        rightMargin:40*Constants.scaleFactorX
        bottomMargin:131.2*Constants.scaleFactorY
    }
    Connections {
        target:SubMenu
        function onCloseItem(name) { if(name===root.propName) root.close() }
    }
}
