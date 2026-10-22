import QtQuick
import QtQuick.Window
import com.hasselblad.proxies
import com.hasselblad.storage
import com.hasselblad.constants

Item {
    id: root
    property var searchRoot: null
    property var popup: null
    property bool showing: false
    readonly property var activeController: controller
    property string selectedExtension: ""
    property int stackMethod: 2
    property bool averageNoiseReduction: false
    property int stackTotalSeconds: 60
    property int stackInitialDelay: 2
    property int stackTargetIso: 100
    property bool selectionPending: false
    readonly property string appliedExtension: controller.applied ? (controller.stackMode ? "stack" : "multishot") : ""
    property bool applyPending: false
    property bool menuApplicationPending: false
    property string menuApplicationError: ""
    property string requestedExtension: ""
    property real applicationDeadline: 0
    property bool openingPage: false
    property bool preparationPage: false
    property bool preparationWaitNotice: false
    property var connectedPopup: null
    property var driveControl: null
    property var factoryExposureModel: null
    property var returnFocusItem: null
    property bool exposureModelAttempted: false
    function connectExposureModel() {
        if(factoryExposureModel || exposureModelAttempted) return
        var main=root.find(root.searchRoot,"MainViewModel_root")
        if(main){factoryExposureModel=main.exposeScreen;root.verifyComponents();return}
        // 非视觉的 MainViewModel 不一定在 Item.children 中；复用控制屏已公开的 MainModel。
        var control=popup ? popup.parent : null
        if(!control || !control.viewModel || !control.viewModel.main) return
        exposureModelAttempted=true
        var component=Qt.createComponent("qrc:/app/qml/viewmodels/ExposeScreenViewModel.qml")
        if(component.status === Component.Ready) {
            factoryExposureModel=component.createObject(root,{main:control.viewModel.main})
            root.verifyComponents()
        } else if(component.status === Component.Error) componentError=component.errorString()
    }
    property var driveControlModel: null
    property string lastStockDriveIcon: "image://svg/ic_controlscreen_drive_mode_single"
    function extendedDriveIcon(model) {
        return controller.applied || (Camera.drive_mode === 6 && (!model || !model.driveModeIcon))
    }
    function driveIcon(model) {
        if (controller.applied)
            return "file:///system/etc/x2d-pixel-overclock/"+(controller.stackMode ? "stack.svg" : "multishot.svg")
        if (model && model.driveModeIcon) return model.driveModeIcon
        if (Camera.drive_mode === 6) return "file:///system/etc/x2d-pixel-overclock/multishot.svg"
        return root.lastStockDriveIcon
    }
    function connectDriveIcon() {
        var control=driveControl || root.find(root.searchRoot,"ControlScreen_driveControl")
        var model=popup && popup.parent ? popup.parent.viewModel : null
        if (!control || !model) return
        if (model.driveModeIcon) root.lastStockDriveIcon=model.driveModeIcon
        if (control === driveControl && model === driveControlModel) return
        var stockScale=control.symbolScale
        // 同一个控件换模型时复用原厂比例，避免反复缩小。
        if (control === driveControl) stockScale=root.driveStockScale
        root.driveStockScale=stockScale
        driveControl=control
        driveControlModel=model
        control.symbol=Qt.binding(function(){return root.driveIcon(root.driveControlModel)})
        control.symbolScale=Qt.binding(function(){return root.driveStockScale*(root.extendedDriveIcon(root.driveControlModel) ? 44/64 : 1)})
    }
    property real driveStockScale: 1
    function connectPopup() {
        if (popup && popup.item && connectedPopup !== popup.item &&
                String(popup.source).endsWith("/X2dPopoverDriveMode.qml")) {
            connectedPopup=popup.item
            connectedPopup.selectedExtension=Qt.binding(function(){return root.selectedExtension})
            connectedPopup.multishotController=controller
            connectedPopup.stackMethod=Qt.binding(function(){return root.stackMethod})
            connectedPopup.averageNoiseReduction=Qt.binding(function(){return root.averageNoiseReduction})
            connectedPopup.stackTotalSeconds=Qt.binding(function(){return root.stackTotalSeconds})
            connectedPopup.stackInitialDelay=Qt.binding(function(){return root.stackInitialDelay})
            connectedPopup.stackTargetIso=Qt.binding(function(){return root.stackTargetIso})
            connectedPopup.stackPlannedFrames=Qt.binding(function(){return controller.stackPlannedFrames})
            connectedPopup.stackFrameCount=Qt.binding(function(){return controller.stackFrameCount})
            connectedPopup.stackGapSeconds=Qt.binding(function(){return controller.stackGapMs/1000})
            connectedPopup.exposureModel=Qt.binding(function(){return root.popup && root.popup.parent ? root.popup.parent.viewModel : null})
        }
    }
    X2dAboutDisplay { searchRoot: root.searchRoot }
    property var popupComponent: null
    property var pageComponent: null
    property bool pageCreated: false
    property string componentError: ""
    property bool routeVerified: false
    property bool parametersVerified: false
    property bool selectorVerified: false
    property bool iconVerified: false
    property int parameterCheckRow: 0
    property var checkModel: null
    property string checkPrevious: ""
    property string lastReport: ""
    property string refreshedAlbum: ""
    property bool storageAvailable: true
    property bool storageSynced: true
    property bool albumNeedsRefresh: false
    property bool albumRefreshPending: false
    property string albumRefreshTarget: ""
    property string albumBeforePreparation: ""
    // 调试候选可短时启用；不访问照片内容，也不操作缩放或屏幕。
    property bool playbackDebugEnabled: true
    Loader {
        id: playbackDiagnostics
        parent: root.Window.window ? root.Window.window.contentItem : root.searchRoot
        active: root.playbackDebugEnabled
        source: "file:///system/etc/x2d-pixel-overclock/PlaybackDiagnostics.qml"
        onLoaded: {
            item.searchRoot=Qt.binding(function(){return root.searchRoot})
            item.channelDirectory=Qt.binding(function(){return controller.channelDirectory})
            item.refreshPending=Qt.binding(function(){return root.albumRefreshPending})
        }
    }
    function viewingNativePhoto() {
        var browse=find(searchRoot,"BrowseView_root")
        var model=browse ? browse.viewModel : null
        return model !== null && model !== undefined && model.browseViewActive &&
                model.currentItemModel !== null && model.currentItemModel !== undefined &&
                model.model && model.model.listSize > 0
    }
    function nativeAlbumPath(value) {
        return /^\/ssd(?:\/[1-9][0-9]{2}HASBL)?$/.test(String(value))
    }
    function rememberAlbumForPreparation() {
        if(nativeAlbumPath(ContentModel.path)) albumBeforePreparation=String(ContentModel.path)
        albumNeedsRefresh=true
    }
    function queueAlbumRefresh(target) {
        if(target && nativeAlbumPath(target)) albumRefreshTarget=target
        albumNeedsRefresh=true
        albumRefreshPending=true
    }
    function refreshNativeAlbum() {
        if(!storageAvailable || !storageSynced || controller.busy || controller.preparingMode ||
                ContentModel.isBrowsing || Storage.browse_active) return
        // 目录重建会销毁当前回放 delegate 并丢失缩放。已有照片正在查看时
        // 保留待刷新请求，退出回放后执行；空相册仍允许立即恢复。
        if(viewingNativePhoto()) return
        var target=albumRefreshTarget || String(ContentModel.path)
        // 存储重连可能清空当前路径；只恢复准备前已存在的原厂目录。
        if(!nativeAlbumPath(target)) target=albumBeforePreparation
        if(!nativeAlbumPath(target)) {albumRefreshPending=false;return}
        albumRefreshPending=false;albumNeedsRefresh=false;albumRefreshTarget=""
        if(playbackDiagnostics.item) playbackDiagnostics.item.note("album-refresh")
        if(String(ContentModel.path)===target) ContentModel.pathChanged(target)
        else ContentModel.enterDir(target)
        ContentModel.updateLoading()
    }
    Timer {
        interval:200; repeat:true
        running:root.albumRefreshPending && root.storageAvailable && root.storageSynced &&
                !controller.busy && !controller.preparingMode
        onTriggered:root.refreshNativeAlbum()
    }
    Connections {
        target:Storage
        function onAvailableChanged(available) {
            root.storageAvailable=available
            if(!available) {root.storageSynced=false;root.albumNeedsRefresh=true}
        }
        function onSynced() {
            root.storageSynced=true
            if(root.albumNeedsRefresh) root.queueAlbumRefresh("")
        }
    }
    function refreshCompletedAlbum(location) {
        var match=/^([1-9][0-9]{2}HASBL)\/[A-Z][0-9]{7}\.(?:JPG|HEIC)$/.exec(String(location))
        if(!match || controller.busy || refreshedAlbum===location) return
        // 文件发布核验成功后更新原厂相册模型；不在空闲时轮询。
        var target="/ssd/"+match[1]
        // 原厂 enterDir 对相同路径直接返回；存储重连后的空缓存需要重新发起 browse。
        queueAlbumRefresh(target)
        refreshedAlbum=location
    }
    // 仅自有诊断文件请求 UI 路由核验；不调用 controller.start 或任何曝光接口。
    function diagnostic() {
        var request=new XMLHttpRequest()
        request.onreadystatechange=function(){
            if(request.readyState !== XMLHttpRequest.DONE || request.responseText !== "CHECK_UI_R6\n") return
            var finish=new XMLHttpRequest();finish.open("PUT",controller.channelDirectory+"route-check.txt");finish.send("CHECK_CONSUMED\n")
            var control=popup ? popup.parent : null
            if(!control || !control.viewModel || !controller.ready || controller.busy || controller.acknowledgement !== 0) return
            checkModel=control.viewModel;checkPrevious=String(checkModel.mainState)
            checkModel.setMainState("drive_mode");routeStep.start()
        }
        request.open("GET",controller.channelDirectory+"route-check.txt");request.send()
    }
    Timer { interval:1500; running:true; repeat:false; onTriggered:root.diagnostic() }
    Timer {
        id:routeStep;interval:200;repeat:false
        onTriggered:{
            if(root.popup && root.popup.item && String(root.popup.source).endsWith("/X2dPopoverDriveMode.qml")) {
                root.connectPopup()
                root.popup.item.selectExtension("multishot")
                if(root.selectedExtension !== "multishot" || root.showing || controller.acknowledgement !== 0) {
                    root.checkModel.setMainState(root.checkPrevious);root.checkModel=null;return
                }
                var icon=root.find(root.popup.item,"PixelOverclockModeIcon")
                root.iconVerified=icon !== null && icon.image.status === Image.Ready
                root.popup.item.openParameters();routeParameters.start()
            } else {root.checkModel.setMainState(root.checkPrevious);root.checkModel=null}
        }
    }
    Timer {
        id:routeParameters;interval:300;repeat:false
        onTriggered:{
            var settings=root.find(root.popup.item,"X2dMultishotSettings_root")
            root.parametersVerified=settings !== null && settings.controller !== null
            if(root.parametersVerified && !settings.openRow(0)){root.parameterCheckRow=1;settings.openRow(1);routeSelector.start()}
            else {root.checkModel.setMainState(root.checkPrevious);root.checkModel=null;root.selectedExtension="";root.report()}
        }
    }
    Timer {
        id:routeSelector;interval:300;repeat:false
        onTriggered:{
            var settings=root.find(root.popup.item,"X2dMultishotSettings_root")
            var selector=root.find(root.popup.item,"ListSelectorSettings_root")
            var row=root.parameterCheckRow
            var expected=["","initialDelay","keepMode"]
            var valid=selector !== null && selector.optionKey === expected[row] && selector.values.length === (row === 1 ? 59 : 2)
            if(!valid){root.selectorVerified=false;root.checkModel.setMainState(root.checkPrevious);root.checkModel=null;root.selectedExtension="";root.report();return}
            selector.itemSelected(row === 1 ? 3 : 1)
            if(row === 1){root.parameterCheckRow=2;settings.openRow(2);routeSelector.restart();return}
            root.selectorVerified=controller.frames === 6 && controller.initialDelay === 5 && controller.saveFormat >= 0 && controller.keepMode
            controller.setOption("initialDelay",2);controller.setOption("keepMode",0)
            routeFinish.start()
        }
    }
    Timer {
        id:routeFinish;interval:200;repeat:false
        onTriggered:{
            root.routeVerified=root.iconVerified && root.parametersVerified && root.selectorVerified && controller.acknowledgement === 0
            root.showing=false
            root.selectedExtension=""
            if(root.checkModel){root.checkModel.setMainState(root.checkPrevious);root.checkModel=null}
            root.report()
        }
    }
    function verifyComponents() {
        popupComponent=Qt.createComponent("X2dPopoverDriveMode.qml")
        pageComponent=Qt.createComponent("PixelOverclockPage.qml")
        if(popupComponent.status === Component.Error) componentError=popupComponent.errorString()
        if(pageComponent.status === Component.Error) componentError+=pageComponent.errorString()
        if(pageComponent.status === Component.Ready && factoryExposureModel) {
            var check=pageComponent.createObject(root,{visible:false,focus:false,width:1024,height:768,controller:controller,viewModel:factoryExposureModel})
            pageCreated=check !== null && check.keyOverride >= 0
            if(check) check.destroy()
        }
    }
    function report() {
        var value="PIXEL_OVERCLOCK_UI_HOST_READY rawStackUI=3 popup="+(popup?1:0)+" page="+(page.item?1:0)+" pageStatus="+page.status+
                  " popupComponent="+(popupComponent?popupComponent.status:-1)+" pageCreated="+(pageCreated?1:0)+
                  " routeVerified="+(routeVerified?1:0)+" parametersVerified="+(parametersVerified?1:0)+
                  " selectorVerified="+(selectorVerified?1:0)+" iconVerified="+(iconVerified?1:0)+" factoryModel="+(factoryExposureModel?1:0)+"\n"+componentError
        if(value === lastReport) return
        lastReport=value
        var request=new XMLHttpRequest()
        request.open("PUT",controller.channelDirectory+"ui.txt")
        request.send(value)
    }
    function find(item,name) {
        if (!item) return null
        if (item.objectName === name) return item
        for (var i=0;i<item.children.length;i++) { var found=find(item.children[i],name); if(found) return found }
        return null
    }
    function redirect() {
        if (popup && String(popup.source).endsWith("/PopoverDriveMode.qml")) {
            popup.source="file:///system/etc/x2d-pixel-overclock/X2dPopoverDriveMode.qml"
            console.info("PIXEL_OVERCLOCK_DRIVE_ROUTE_CONNECTED")
        }
    }
    function open() {
        if(!showing && root.Window.window) returnFocusItem=root.Window.window.activeFocusItem
        openingPage=true
        if (popup && popup.item) popup.item.closeDriveMode()
        openingPage=false
        showing=true
        Qt.callLater(function(){ if(page.item) page.item.forceActiveFocus() })
    }
    function applySelectedMode() {
        if (controller.busy || (selectedExtension !== "multishot" && selectedExtension !== "stack")) return
        requestedExtension=selectedExtension
        menuApplicationError=""
        selectionPending=true
        // 首次打开菜单的异步回读尚未结束时，不丢弃应用意图。
        menuApplicationPending=true
        applicationDeadline=Date.now()+2500
        controller.refresh()
        controller.refreshShutter()
        tryMenuApplication()
    }
    function tryMenuApplication() {
        if (!menuApplicationPending) return
        if (controller.busy || requestedExtension !== selectedExtension) {
            menuApplicationPending=false
            return
        }
        if (!(requestedExtension === "multishot" && controller.pixelResolution === 25 ? controller.applicationReady : controller.canStart && controller.nativeReady)) return
        menuApplicationPending=false
        performSelectedModeApplication()
    }
    Timer {
        interval:50; repeat:true; running:root.menuApplicationPending
        onTriggered: {
            if (Date.now() >= root.applicationDeadline) {
                root.menuApplicationPending=false
                root.menuApplicationError="模式应用失败：拍摄后端未就绪"
                controller.lastError=root.menuApplicationError
                controller.stage="failed"
                root.open()
            } else root.tryMenuApplication()
        }
    }
    function performSelectedModeApplication() {
        if(!(selectedExtension === "multishot" && controller.pixelResolution === 25)) rememberAlbumForPreparation()
        if(selectedExtension === "stack") applyPending=controller.applyStack(stackMethod,stackTotalSeconds,averageNoiseReduction,stackInitialDelay,stackTargetIso)
        else {controller.stackMode=false;applyPending=controller.applyMode()}
        if(!applyPending) {
            menuApplicationError=controller.lastError || "模式应用失败"
            root.open()
            return
        }
        if(applyPending) selectionPending=false
        if(applyPending && controller.quarterMode){
            openingPage=true
            if(popup && popup.item){popup.item.setDriveMode(0);popup.item.setExitDriveMode(0);popup.item.closeDriveMode()}
            openingPage=false;preparationPage=false;applyPending=false;dismissProcessingPage();return
        }
        if(applyPending && controller.preparingMode && !preparationPage){
            if(showing) dismissProcessingPage()
            preparationPage=true
            openingPage=true
            if(popup && popup.item) popup.item.closeDriveMode()
            openingPage=false
        }
    }
    function dismissProcessingPage() {
        if(controller.busy) return
        var ownedFocus=root.showing
        // Loader 销毁原厂曝光 FocusScope 后不会自动恢复先前页面的焦点。
        // 先撤销页面焦点，再等卸载完成后交还；不硬写全局按键转发参数。
        if(page.item) page.item.focus=false
        showing=false
        // 底部准备文字没有取得焦点，不把用户已转到的原厂页面抢回来。
        if(!ownedFocus) return
        Qt.callLater(function(){
            if(root.showing || controller.busy) return
            var target=root.returnFocusItem
            if(!target || !target.visible || !target.enabled)
                target=root.find(root.searchRoot,"ControlScreen_root") ||
                        root.find(root.searchRoot,"ControlScreen_focusScope") ||
                        (root.popup ? root.popup.parent : null)
            root.returnFocusItem=null
            if(target) target.forceActiveFocus()
            root.reportFocusState()
        })
    }
    function reportFocusState() {
        var item=root.Window.window ? root.Window.window.activeFocusItem : null
        var request=new XMLHttpRequest()
        request.open("PUT",controller.channelDirectory+"focus-state.txt")
        request.send("PIXEL_FOCUS showing="+(root.showing?1:0)+" busy="+(controller.busy?1:0)+
                     " preparing="+(controller.preparingMode?1:0)+" prepared="+(controller.preparedMode?1:0)+
                     " override="+Camera.forward_input_events+" afBlocked="+Camera.af_blocked+
                     " item="+(item ? item.objectName : "")+"\n")
    }
    PixelOverclockController {
        id: controller; cameraProxy: Camera
        monitoringRequested: root.showing || root.menuApplicationPending || (root.popup !== null && root.popup.active === true &&
            String(root.popup.source).endsWith("/X2dPopoverDriveMode.qml"))
    }
    Timer {
        // 仅启动时等待原厂控制屏创建；找到对象后改用 Loader 信号，不再遍历整棵界面树。
        interval:250; running:!root.popup || !root.driveControl; repeat:true; triggeredOnStart:true
        onTriggered: { if (!root.popup) root.popup=root.find(root.searchRoot,"ControlScreen_popupLoader"); root.redirect(); root.connectExposureModel(); root.connectPopup(); root.connectDriveIcon(); root.report() }
    }
    Connections {
        target:root.popup
        function onSourceChanged(){root.redirect()}
        function onLoaded(){root.connectExposureModel();root.connectPopup();root.connectDriveIcon();root.report()}
    }
    Connections {
        target:root.popup ? root.popup.item : null
        ignoreUnknownSignals:true
        function onSelectExtension(mode) {
            root.selectedExtension=mode
            root.selectionPending=mode === "multishot" || mode === "stack"
            if(!mode) {root.menuApplicationError="";root.menuApplicationPending=false;root.applyPending=false;controller.disarmMode()}
        }
        function onApplyExtension(mode) { if((mode === "multishot" || mode === "stack") && !root.checkModel && !root.openingPage) {root.selectedExtension=mode;root.applySelectedMode()} }
        function onOpenExtension(mode) {
            if(mode === "multishot") root.open()
            else if(mode === "stack" && root.popup && root.popup.item) root.popup.item.openParameters()
        }
        function onStackMethodChosen(value) { root.stackMethod=value }
        function onAverageNoiseReductionChosen(value) { root.averageNoiseReduction=value }
        function onStackTotalSecondsChosen(value) { root.stackTotalSeconds=value }
        function onStackInitialDelayChosen(value) { root.stackInitialDelay=value }
        function onStackTargetIsoChosen(value) { root.stackTargetIso=value }
        // 原厂返回、点菜单外部、退出菜单都发 closePopup；仅应用模式，绝不触发曝光。
        function onClosePopup() {
            if(root.popup && root.popup.item && root.popup.item.handlesExitApplication === true) return
            if((root.selectedExtension === "multishot" || root.selectedExtension === "stack") && !controller.busy && !root.checkModel && !root.openingPage)
                root.applySelectedMode()
        }
    }
    Connections {
        target:controller
        function onPreparingModeChanged(){if(controller.preparingMode)root.rememberAlbumForPreparation();else root.preparationWaitNotice=false}
        function onPreparationWaitRequested(){if(controller.applied && controller.preparingMode){root.preparationWaitNotice=true;preparationWaitTimer.restart()}}
        function onPreparedModeChanged(){if(controller.preparedMode && !controller.quarterMode)root.queueAlbumRefresh("")}
        function onStageChanged(){if(!controller.stackMode && controller.pixelResolution === 25 && controller.stage === "failed") root.open()}
        function onBusyChanged(){if(controller.busy){root.selectionPending=false;root.preparationPage=false;root.applyPending=false;root.selectedExtension=controller.stackMode ? "stack" : "multishot";if(controller.stackMode || controller.pixelResolution !== 25) root.open()}}
        function onAppliedChanged(){
            if(controller.applied) root.selectedExtension=controller.stackMode ? "stack" : "multishot"
            else if(!root.selectionPending) root.selectedExtension=""
            if(!controller.applied){
                root.applyPending=false
                if(root.preparationPage){
                    root.preparationPage=false
                    if(controller.stage === "failed") root.open()
                    else root.dismissProcessingPage()
                }
            }
        }
        function onAppliedConfirmedChanged(){if(controller.appliedConfirmed && root.preparationPage){root.applyPending=false;root.preparationPage=false;root.dismissProcessingPage();root.reportFocusState()}}
        function onCompleted(albumLocation){root.selectionPending=false;root.refreshCompletedAlbum(albumLocation);root.selectedExtension=controller.keepMode ? "multishot" : "";root.applyPending=false;root.dismissProcessingPage()}
    }
    Timer {id:preparationWaitTimer;interval:2000;onTriggered:root.preparationWaitNotice=false}
    Item {
        id:preparationIndicator
        objectName:"PixelOverclockPreparationBubble"
        parent:root.Window.window ? root.Window.window.contentItem : root.searchRoot
        anchors.horizontalCenter:parent ? parent.horizontalCenter : undefined
        anchors.bottom:parent ? parent.bottom : undefined
        anchors.bottomMargin:160*Constants.scaleFactorY
        width:preparationText.implicitWidth
        height:preparationText.implicitHeight
        z:1201
        visible:parent !== null && root.preparationPage && controller.preparingMode && !controller.busy
        Text {
            id:preparationText
            objectName:"PixelOverclockPreparationText"
            anchors.centerIn:parent
            text:controller.shutterPending ? "正在准备，完成后自动拍摄" : root.preparationWaitNotice ? "正在准备，请稍后" : "正在准备"
            color:Constants.colorWhite
            font.pixelSize:24*1.6*Constants.scaleFactor
            font.weight:Font.Normal
            style:Text.Outline
            styleColor:Constants.colorBlack
        }
        // 不接管焦点或触摸；真正就绪后整个提示立即隐藏。
    }
    Loader {
        id:page; objectName:"PixelOverclockPageLoader"
        parent:root.Window.window ? root.Window.window.contentItem : root.searchRoot
        anchors.fill:parent; z:1200; active:root.showing && parent !== null && root.factoryExposureModel !== null
        sourceComponent: Component { PixelOverclockPage {controller:root.activeController;viewModel:root.factoryExposureModel;applicationError:root.menuApplicationError} }
        onLoaded:{item.forceActiveFocus(); root.report(); console.info("PIXEL_OVERCLOCK_PAGE_READY")}
        onStatusChanged:root.report()
    }
    function retainProcessingFocus() {
        if (!showing || !page.item || !root.Window.window) return
        var item=root.Window.window.activeFocusItem
        while(item) { if(item === page.item) return; item=item.parent }
        Qt.callLater(function(){if(root.showing && page.item)page.item.forceActiveFocus()})
    }
    Connections {
        target:root.Window.window
        function onActiveFocusItemChanged() { root.retainProcessingFocus() }
    }
    Connections {
        target:page.item
        function onClose(){if(root.preparationPage){controller.disarmMode();root.preparationPage=false}root.dismissProcessingPage()}
    }
    Component.onCompleted:{root.verifyComponents();console.info("PIXEL_OVERCLOCK_UI_HOST_READY")}
    onSearchRootChanged: root.verifyComponents()
}
