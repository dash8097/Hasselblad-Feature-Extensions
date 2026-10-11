import QtQuick 2.5
Item {
    id:root
    property bool passed:false
    property int opened:0
    property int step:0
    property var first:null
    PagePool {
        id:pool
        onLoaded:{
            if(!pageItem)throw new Error("null delivered page")
            pageItem.residentActivate()
            root.opened++
        }
    }
    Component.onCompleted:pool.warm("first","PoolDummy.qml",{},true)
    Timer {
        interval:100;repeat:true;running:!root.passed
        onTriggered:{
            if(pool.isLoading())return
            switch(root.step++) {
            case 0:pool.setPage("first","PoolDummy.qml",{},true);pool.active=true;break
            case 1:
                if(root.opened!==1 || !pool.item.activated)throw new Error("first activation failed")
                root.first=pool.item;pool.active=false;break
            case 2:pool.setPage("second","PoolDummy.qml",{},false);pool.active=true;break
            case 3:
                if(root.opened!==2 || !pool.item.activated)throw new Error("second activation failed")
                pool.active=false;break
            case 4:
                if(pool.slots.second)throw new Error("transient page retained")
                pool.setPage("first","PoolDummy.qml",{},true);pool.active=true;break
            case 5:
                if(root.opened!==3 || pool.item!==root.first)throw new Error("retained page failed")
                root.passed=true;console.log("PASS pool runtime: warm, lazy, reopen, identity, dispatch");Qt.quit();break
            }
        }
    }
}
