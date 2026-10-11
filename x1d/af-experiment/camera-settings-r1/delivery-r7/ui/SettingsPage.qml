import QtQuick 2.5

Rectangle {
    id: page
    objectName: "AfSettingsPage"
    color: "black"
    property bool pageActive: false
    property var backend: typeof hblAf === "undefined" ? null : hblAf
    property real unit: Math.min(width/640,height/480)
    property var probeSpeeds: [5000,7000,9000,11000,13000,15000,17000,20000]
    property var fastSpeeds: [5000,10000,15000,20000]
    property var fineSpeeds: [3000,4000,5000,6000,7000]
    property int startSpeed: 0
    property int startSamples: 3
    property var draft: [0,0,0]
    property var advance: [65534,65534]
    property int loadedRevision: 0
    property bool dirty: false
    property var touched: [false,false,false,false,false,false,false,false]
    readonly property bool connected: backend && backend.connected
    readonly property bool applying: backend && backend.applying
    readonly property bool reading: backend && backend.reading
    readonly property bool busy: applying
    signal backRequested()
    function command(value) { if(backend) backend.command=JSON.stringify(value) }
    function touch(index) { var next=touched.slice(0);next[index]=true;touched=next;dirty=true;command({op:"localEdit"}) }
    function visibilityCommand() { command({op:"visible",value:pageActive,width:Math.round(width),height:Math.round(height)}) }
    function adoptInitial() {
        if(!connected || loadedRevision!==0)return
        var next=draft.slice(0),latest=[backend.probe,backend.fast,backend.fine],adv=advance.slice(0)
        for(var i=0;i<3;i++)if(!touched[i])next[i]=latest[i]
        if(!touched[4])adv[0]=backend.fastAdvanceMs
        if(!touched[5])adv[1]=backend.fineAdvanceMs
        if(!touched[6])startSpeed=backend.startSpeed
        if(!touched[7])startSamples=backend.startSamples
        draft=next;advance=adv;loadedRevision=backend.revision
    }
    function speedText(value) { return value===0?"跟随原厂":String(value) }
    function startText(value) { return value===0?"关闭":value===65533?"镜头报告×70%":String(value) }
    function factorySpeed(index) {
        var next=draft.slice(0);next[index]=0;draft=next
        if(index>0){var updated=advance.slice(0);updated[index-1]=65534;advance=updated;touch(index+3)}
        touch(index)
    }
    function reloadDraft() {
        if(!connected)return
        draft=[backend.probe,backend.fast,backend.fine]
        startSpeed=backend.startSpeed;startSamples=backend.startSamples
        advance=[backend.fastAdvanceMs,backend.fineAdvanceMs]
        loadedRevision=backend.revision;dirty=false;touched=[false,false,false,false,false,false,false,false]
    }
    function changeSpeed(index,delta) {
        var next=draft.slice(0),choices=index===0?probeSpeeds:(index===1?fastSpeeds:fineSpeeds)
        var target=next[index],i
        if(target===0)target=choices[0]
        else {
        if(delta>0){for(i=0;i<choices.length;i++)if(choices[i]>target){target=choices[i];break}}
        else {for(i=choices.length-1;i>=0;i--)if(choices[i]<target){target=choices[i];break}}
        }
        if(target===next[index])return
        next[index]=target;draft=next;touch(index)
        if(index>0){
            var updated=advance.slice(0),presets=backend?(index===1?backend.fastPresets:backend.finePresets):null
            var preset=presets?presets[choices.indexOf(target)]:65535
            updated[index-1]=typeof preset==="number"?preset:65535;advance=updated;touch(index+3)
        }
    }
    function changeAdvance(index,delta) {
        var next=advance.slice(0)
        next[index]=next[index]>=65534?0:Math.max(0,Math.min(200,next[index]+delta));advance=next;touch(index+4)
    }
    onPageActiveChanged: { visibilityCommand();adoptInitial() }
    onWidthChanged: if(pageActive)visibilityCommand()
    onHeightChanged: if(pageActive)visibilityCommand()
    onConnectedChanged: adoptInitial()
    Component.onCompleted: { visibilityCommand();adoptInitial() }
    Component.onDestruction: command({op:"visible",value:false})
    Timer {
        interval: 200; running: page.pageActive; repeat: true
        onTriggered: {
            page.adoptInitial()
            if(page.connected && page.backend.canApply && !page.applying && page.loadedRevision>0 && page.dirty && page.draft[0]===page.backend.probe && page.draft[1]===page.backend.fast &&
               page.draft[2]===page.backend.fine && 
               page.startSpeed===page.backend.startSpeed && page.startSamples===page.backend.startSamples &&
               page.advance[0]===page.backend.fastAdvanceMs && page.advance[1]===page.backend.fineAdvanceMs)page.dirty=false
            if(page.connected && !page.dirty && page.loadedRevision!==page.backend.revision)page.reloadDraft()
        }
    }
    Item {
        width: 640; height: 480; scale: page.unit; transformOrigin: Item.TopLeft
        x: (parent.width-640*page.unit)/2; y:(parent.height-480*page.unit)/2
        Text { x:24; y:14; text:"AF 设置"; color:"white"; font.pixelSize:28 }
        Text { x:24; y:51; text:"速度为命令幅值 · 精扫提前量为确认等待预算"; color:"#999999"; font.pixelSize:16 }
        Rectangle {
            x:556; y:14; width:60; height:42; radius:4; color:"#222222"
            Text { anchors.centerIn:parent; text:"返回"; color:"white"; font.pixelSize:18 }
            MouseArea { objectName:"AfBackButton"; anchors.fill:parent; onClicked:page.backRequested() }
        }
        Flickable {
            id: settingsScroll
            objectName: "AfSettingsScroll"
            x:24; y:82; width:592; height:294; clip:true
            contentWidth:width;contentHeight:settingsColumn.height
            flickableDirection:Flickable.VerticalFlick;boundsBehavior:Flickable.StopAtBounds
        Column {
            id: settingsColumn
            spacing:8
            Rectangle {
                width:592;height:96;color:"#171717";radius:4
                Text { x:14;y:5;text:"低速起步";color:"white";font.pixelSize:21 }
                Text { x:14;y:32;text:page.connected?"本轮 "+page.startText(page.backend.activeStartSpeed)+(page.backend.activeStartSpeed===0?"":" · "+page.backend.activeStartSamples+" 个采样"):"本轮 未读回";color:"#969696";font.pixelSize:13 }
                Rectangle {
                    x:143;y:4;width:151;height:26;radius:3;color:page.startSpeed===65533?"#245c70":"#2b2b2b"
                    Text { anchors.centerIn:parent;text:"镜头报告 ×70%";color:"white";font.pixelSize:16 }
                    MouseArea { objectName:"AfStartAuto";anchors.fill:parent;enabled:!page.applying
                        onClicked:{page.startSpeed=65533;page.startSamples=10;page.touch(6);page.touch(7)} }
                }
                Repeater {
                    model:2
                    Rectangle {
                        x:index===0?312:521;y:4;width:55;height:44;color:"#2b2b2b";radius:3
                        Text { anchors.centerIn:parent;text:index===0?"−":"+";color:"white";font.pixelSize:27 }
                        MouseArea { objectName:index===0?"AfStartSpeedMinus":"AfStartSpeedPlus";anchors.fill:parent;enabled:!page.applying
                            onClicked:{page.startSpeed=page.startSpeed===65533?1000:Math.max(0,Math.min(5000,page.startSpeed+(index===0?-1000:1000)));page.touch(6)} }
                    }
                }
                Text { x:370;width:145;height:52;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter;text:page.startText(page.startSpeed);color:"white";font.pixelSize:page.startSpeed===65533?18:27 }
                Text { x:14;y:64;text:"有效采样数";color:"#bbbbbb";font.pixelSize:18 }
                Repeater {
                    model:2
                    Rectangle {
                        x:index===0?312:521;y:52;width:55;height:40;color:"#2b2b2b";radius:3
                        Text { anchors.centerIn:parent;text:index===0?"−":"+";color:"white";font.pixelSize:25 }
                        MouseArea { objectName:index===0?"AfStartSamplesMinus":"AfStartSamplesPlus";anchors.fill:parent;enabled:!page.applying
                            onClicked:{page.startSamples=Math.max(1,Math.min(500,page.startSamples+(index===0?-1:1)));page.touch(7)} }
                    }
                }
                Text { x:370;y:52;width:145;height:40;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter;text:page.startSamples;color:"white";font.pixelSize:25 }
            }
            Repeater {
                model: ["高速段","快速扫描","原厂精扫"]
                Rectangle {
                    width:592; height:index===0?52:88; color:"#171717"; radius:4
                    Text { x:14; y:5; text:modelData; color:"white"; font.pixelSize:21 }
                    Text {
                        x:14; y:31; color:"#969696"; font.pixelSize:13
                        text:page.connected ? "本轮配置 "+page.speedText([page.backend.activeProbe,page.backend.activeFast,page.backend.activeFine][index]) : "本轮配置 未读回"
                    }
                    Text {
                        x:215; y:30; text:"恢复原厂"; color:"#bbbbbb"; font.pixelSize:13
                        MouseArea { anchors.fill:parent; anchors.margins:-4; enabled:!page.applying; onClicked:page.factorySpeed(index) }
                    }
                    Rectangle {
                        x:312; y:4; width:55; height:44; color:"#2b2b2b"; radius:3
                        Text { anchors.centerIn:parent; text:page.draft[index]===0?"手动":"−"; color:"white"; font.pixelSize:page.draft[index]===0?16:27 }
                        MouseArea { objectName:"AfSpeedMinus"+index; anchors.fill:parent; enabled:!page.applying; onClicked:page.changeSpeed(index,-1) }
                    }
                    Text { x:370; width:145; height:52; horizontalAlignment:Text.AlignHCenter; verticalAlignment:Text.AlignVCenter; text:page.speedText(page.draft[index]); color:"white"; font.pixelSize:page.draft[index]===0?23:27 }
                    Rectangle {
                        x:521; y:4; width:55; height:44; color:"#2b2b2b"; radius:3
                        Text { anchors.centerIn:parent; text:page.draft[index]===0?"手动":"+"; color:"white"; font.pixelSize:page.draft[index]===0?16:27 }
                        MouseArea { objectName:"AfSpeedPlus"+index; anchors.fill:parent; enabled:!page.applying; onClicked:page.changeSpeed(index,1) }
                    }
                    Item {
                        x:14; y:53; width:562; height:31; visible:index>0
                        property int advanceIndex: Math.max(0,index-1)
                        Text { y:5; text:(index===1?"切换提前补偿":"停止提前补偿")+(page.advance[parent.advanceIndex]===65534?" · 保留原厂行为":" · 仅保存，未接通"); color:"#999999"; font.pixelSize:13 }
                        Rectangle {
                            x:298; width:55; height:31; color:"#252525"; radius:3
                            Text { anchors.centerIn:parent; text:"−"; color:"#aaaaaa"; font.pixelSize:21 }
                            MouseArea { anchors.fill:parent; enabled:!page.applying; onClicked:page.changeAdvance(index-1,-5) }
                        }
                        Text { x:356; width:145; height:31; horizontalAlignment:Text.AlignHCenter; verticalAlignment:Text.AlignVCenter; text:page.advance[parent.advanceIndex]===65535?"待估算":(page.advance[parent.advanceIndex]===65534?"原厂":page.advance[parent.advanceIndex]+" ms"); color:"#bbbbbb"; font.pixelSize:20 }
                        Rectangle {
                            x:507; width:55; height:31; color:"#252525"; radius:3
                            Text { anchors.centerIn:parent; text:"+"; color:"#aaaaaa"; font.pixelSize:21 }
                            MouseArea { anchors.fill:parent; enabled:!page.applying; onClicked:page.changeAdvance(index-1,5) }
                        }
                    }
                }
            }

        }
        }
        Text {
            x:24; y:385; width:592; elide:Text.ElideRight; font.pixelSize:15
            color:page.connected?"#bbbbbb":"#d8a77c"
            text:page.backend?page.backend.statusText:"AF 接口未加载"
        }
        Rectangle {
            x:24; y:418; width:172; height:44; radius:4; color:"#222222"
            Text { anchors.centerIn:parent; text:"重新读取"; color:"white"; font.pixelSize:19 }
            MouseArea { objectName:"AfReadButton"; anchors.fill:parent; enabled:!page.applying; onClicked:{page.dirty=false;page.touched=[false,false,false,false,false,false,false,false];page.loadedRevision=0;page.command({op:"read"})} }
        }
        Text { x:211; y:426; width:195; text:page.dirty?"有未保存修改":""; color:"#a9a9a9"; font.pixelSize:17 }
        Rectangle {
            x:414; y:418; width:202; height:44; radius:4
            color:page.connected && page.backend.canApply && !page.applying && page.loadedRevision>0 && page.dirty?"#eeeeee":"#333333"
            Text { anchors.centerIn:parent; text:page.applying?"等待相机确认":"保存配置"; color:page.connected && page.backend.canApply && !page.applying && page.loadedRevision>0 && page.dirty?"black":"#aaaaaa"; font.pixelSize:19 }
            MouseArea {
                objectName:"AfApplyButton"
                anchors.fill:parent; enabled:page.connected && page.backend.canApply && !page.applying && page.loadedRevision>0 && page.dirty
                onClicked:page.command({op:"apply",expectedRevision:page.loadedRevision,probe:page.draft[0],fast:page.draft[1],fine:page.draft[2],newDirection:false,fastAdvanceMs:page.advance[0],fineAdvanceMs:page.advance[1],startSpeed:page.startSpeed,startSamples:page.startSamples})
            }
        }
    }
}
