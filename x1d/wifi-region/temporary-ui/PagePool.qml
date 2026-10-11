import QtQuick 2.5
FocusScope {
    id:host
    property bool active:false
    property bool asynchronous:true
    property var slots:({})
    property var selected:null
    property bool delivered:false
    property var item:active && selected?selected.item:null
    property int status:!active?Loader.Null:selected?selected.status:Loader.Loading
    signal loaded(var pageItem)
    signal loadFailed(string reason)
    signal backRequested()
    signal controlRequested()
    function isLoading(){for(var k in slots)if(slots[k].status===Loader.Loading)return true;return false}
    function acquire(key,url,properties,keep) {
        if(slots[key])return slots[key]
        var slot=factory.createObject(host,{cacheKey:key,retained:keep})
        var next={};for(var k in slots)next[k]=slots[k];next[key]=slot;slots=next
        slot.setSource(url,properties)
        return slot
    }
    function warm(key,url,properties){acquire(key,url,properties,true)}
    function setPage(key,url,properties,keep){selected=acquire(key,url,properties,keep)}
    function deliver(){
        // Pass the Loader's object directly; the public item binding may still
        // be pending when active changes on the camera's Qt 5 runtime.
        if(active && selected && selected.item && selected.status===Loader.Ready && !delivered){
            delivered=true;loaded(selected.item)
        }
    }
    Timer {id:dispatch;interval:0;onTriggered:host.deliver()}
    onActiveChanged:{
        delivered=false
        if(active)dispatch.restart()
        else if(selected && !selected.retained){
            var old=selected;selected=null;var next={}
            for(var k in slots)if(k!==old.cacheKey)next[k]=slots[k]
            slots=next;old.destroy()
        }
    }
    Component {
        id:factory
        Loader {
            id:slot
            property string cacheKey
            property bool retained:false
            // Original special pages call these inherited loader functions.
            function closeMenu(){host.backRequested()}
            function closeMainScreen(){host.controlRequested()}
            anchors.fill:parent;active:true;asynchronous:host.asynchronous
            visible:host.active && host.selected===slot
            enabled:visible
            onLoaded:{
                try {
                    if(typeof item.residentPrepare==="function")item.residentPrepare()
                    if(typeof item.residentDeactivate==="function")item.residentDeactivate()
                } catch(error) {
                    console.log("HBL page preparation failed",cacheKey,String(error))
                    if(host.active && host.selected===slot)host.loadFailed(String(error))
                    return
                }
                if(host.selected===slot)dispatch.restart()
            }
        }
    }
}
