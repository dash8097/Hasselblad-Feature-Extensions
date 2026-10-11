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
    function nativePool(operation,args) {
        _hblPagePoolCore.request=[host,operation,args]
        return _hblPagePoolCore.result
    }
    function isLoading(){return nativePool(0,[])}
    function acquire(key,url,properties,keep){return nativePool(1,[key,url,properties,keep])}
    function warm(key,url,properties){nativePool(2,[key,url,properties])}
    function setPage(key,url,properties,keep){nativePool(3,[key,url,properties,keep])}
    function deliver(){nativePool(4,[])}
    onActiveChanged:nativePool(5,[])
    onSelectedChanged:nativePool(6,[selected])
    Component.onDestruction:nativePool(9,[])
    function _createSlot(key,keep){return factory.createObject(host,{cacheKey:key,retained:keep})}
    Component {
        id:factory
        Loader {
            id:slot
            property string cacheKey
            property bool retained:false
            function closeMenu(){host.backRequested()}
            function closeMainScreen(){host.controlRequested()}
            function _setSource(url,properties){setSource(url,properties)}
            anchors.fill:parent;active:true;asynchronous:host.asynchronous
            visible:host.active && host.selected===slot
            enabled:visible
            onLoaded:host.nativePool(7,[slot])
            onStatusChanged:host.nativePool(8,[slot])
        }
    }
}
