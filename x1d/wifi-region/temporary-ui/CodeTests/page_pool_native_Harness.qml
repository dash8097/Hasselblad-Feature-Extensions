import QtQuick 2.5
Item {
    id:root
    property var pool:null
    property var saved:null
    property var callbacks:[]
    property var used:({})
    property var records:[]
    property int created:0
    property int opened:0
    property int failures:0
    property int activationFailures:0
    property bool opening:false
    property bool guardFailure:true
    function record(phase,token){records.push([phase,token])}
    function callback(phase,token) {
        for(var i=0;i<callbacks.length;i++) {
            var cb=callbacks[i]
            if(!used[i] && cb.phase===phase && (typeof cb.token==='undefined' || cb.token===token)) {
                used[i]=true
                for(var j=0;j<cb.events.length;j++)run(cb.events[j])
            }
        }
    }
    function properties(event) {
        var props={probe:root,token:event.key}
        if(event.properties)for(var key in event.properties)props[key]=event.properties[key]
        return props
    }
    function run(event) {
        switch(event.op) {
        case 'configure':callbacks=event.callbacks || [];pool.asynchronous=event.asynchronous===true;break
        case 'set':pool[event.property]=event.value;break
        case 'warm':pool.warm(event.key,event.url || 'Dummy.qml',properties(event));break
        case 'select':pool.setPage(event.key,event.url || 'Dummy.qml',properties(event),event.keep!==false);break
        case 'open':opening=true;pool.setPage(event.key,event.url || 'Dummy.qml',properties(event),event.keep!==false);pool.active=true;break
        case 'close':if(pool.item)pool.item.residentDeactivate();pool.active=false;opening=false;break
        case 'deliver':pool.deliver();break
        case 'save':saved=pool.selected;break
        case 'selectExisting':pool.selected=pool.slots[event.key] || null;break
        case 'loaded':if(event.saved){if(saved)saved.loaded()}else pool.slots[event.key].loaded();break
        case 'exit':pool.item.originalExit();break
        case 'control':pool.item.originalControl();break
        case 'destroySlot':pool.slots[event.key].destroy();break
        case 'destroy':pool.destroy();break
        case 'settle':break
        default:throw new Error('Unknown fixture event '+event.op)
        }
    }
    function observe() {
        var state={live:pool!==null,savedLive:saved!==null,created:created,opened:opened,failures:failures,
            activationFailures:activationFailures,opening:opening,records:records}
        if(pool) {
            state.active=pool.active;state.delivered=pool.delivered;state.status=pool.status
            state.selected=pool.selected?pool.selected.cacheKey:null
            state.item=pool.item?pool.item.token:null;state.loading=pool.isLoading()
            state.keys=Object.keys(pool.slots).sort();state.slots={}
            for(var i=0;i<state.keys.length;i++) {
                var key=state.keys[i],slot=pool.slots[key]
                if(!slot){state.slots[key]=null;continue}
                var page=slot.item
                state.slots[key]={retained:slot.retained,status:slot.status,visible:slot.visible,enabled:slot.enabled,
                    page:page?{token:page.token,serial:page.serial,prepared:page.prepared,
                        deactivated:page.deactivated,activated:page.activated,initial:page.initial}:null}
            }
        }
        return JSON.stringify(state)
    }
    Component {
        id:factory
        POOL_TYPE {
            onLoaded:{
                if(!pageItem)throw new Error('null delivered page')
                root.opened++;root.record('loaded',pageItem.token)
                try{pageItem.residentActivate()}
                catch(error){root.activationFailures++;root.opening=false;active=false;return}
                root.opening=false;root.callback('loaded',pageItem.token)
            }
            onLoadFailed:{
                root.failures++;root.record('failed',selected?selected.cacheKey:'')
                root.callback('failed',selected?selected.cacheKey:'')
                if(root.guardFailure){root.opening=false;active=false}
            }
            onStatusChanged:if(status===Loader.Error && root.guardFailure){
                root.record('statusError',selected?selected.cacheKey:'');root.opening=false;active=false
            }
            onBackRequested:root.record('back','')
            onControlRequested:root.record('control','')
        }
    }
    Component.onCompleted:pool=factory.createObject(root)
}
