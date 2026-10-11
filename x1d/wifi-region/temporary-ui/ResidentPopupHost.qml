import QtQuick 2.5

// Preserve the parameter controller's Loader contract while keeping rendered
// popovers alive. Background creation must not write settings or take focus.
Item {
    id:host
    property url source:""
    property Component sourceComponent:null
    property bool active:false
    property Item item:null
    readonly property int status:item && active?Loader.Ready:Loader.Null
    property var retained:({})
    property var pending:({})
    property var failed:({})
    property var preloadSources:[]
    property int preloadIndex:0
    signal loaded()
    onSourceChanged:activateLater.restart()
    onActiveChanged:activateLater.restart()
    function acquire(url){
        var key=String(url)
        if(failed[key])return null
        if(retained[key])return retained[key]
        var component=pending[key]
        if(!component){component=Qt.createComponent(url,Component.Asynchronous);pending[key]=component}
        if(component.status===Component.Loading)return null
        if(component.status===Component.Error){failed[key]=true;console.warn("Resident parameter component failed",component.errorString());return null}
        var object=component.createObject(host,{preloadOnly:true,visible:false})
        if(object){retained[key]=object;delete pending[key]}
        return object
    }
    function activate(){
        if(!active || String(source)===""){
            var previous=item;item=null
            if(previous && previous.visible)previous.close()
            return
        }
        var next=acquire(source)
        if(!next){if(!failed[String(source)])activateLater.restart();return}
        if(item===next && item.visible)return
        var previousItem=item;item=null
        if(previousItem && previousItem.visible)previousItem.close()
        item=next
        item.present()
        loaded()
    }
    Timer {id:activateLater;interval:16;onTriggered:host.activate()}
    Timer {
        interval:25;repeat:true;running:host.preloadIndex<host.preloadSources.length
        onTriggered:if(host.acquire(host.preloadSources[host.preloadIndex]) || host.failed[String(host.preloadSources[host.preloadIndex])])host.preloadIndex++
    }
}
