import QtQuick 2.5
Rectangle {
 id: page
 width: 640; height: 480; color: "#101316"
 property bool caps: false
 property int keyboardMode: 0
 property var keys: keyboardMode===0 ? (caps?"QWERTYUIOPASDFGHJKLZXCVBNM":"qwertyuiopasdfghjklzxcvbnm").split("") : keyboardMode===1 ? "1234567890!@#$%^&*()-_=+.,?/:;".split("") : "[]{}<>\\|~`\"'()+=!?@#$%&*-_:/.".split("")
 MouseArea {anchors.fill:parent}
 Text {x:16;y:12;text:"连接手机热点";color:"white";font.pixelSize:23}
 Row {x:286;y:7;spacing:8
  Repeater {model:["启用","扫描","返回"]
   Rectangle {width:106;height:40;color:mouse.pressed?"#465665":"#29343e";radius:3
    Text {anchors.centerIn:parent;text:modelData;color:"white";font.pixelSize:20}
    MouseArea {id:mouse;anchors.fill:parent;enabled:!wifi.busy;onClicked:wifi.action=["start","scan","close"][index]}
   }
  }
 }
 Text {x:16;y:57;width:608;height:34;text:wifi.status;color:"#b7c6cf";font.pixelSize:17;wrapMode:Text.Wrap}
 ListView {
  id: networks;x:16;y:98;width:608;height:90;clip:true;spacing:3;model:wifi.networks
  boundsBehavior:Flickable.StopAtBounds
  delegate:Rectangle {width:networks.width;height:43;color:wifi.ssid===modelData?"#304c60":"#1d252c";radius:2
   Text {x:12;anchors.verticalCenter:parent.verticalCenter;width:parent.width-58;text:modelData;color:"white";font.pixelSize:21;elide:Text.ElideRight}
   Rectangle {anchors.right:parent.right;anchors.rightMargin:16;anchors.verticalCenter:parent.verticalCenter;width:12;height:12;radius:6;visible:wifi.ssid===modelData;color:"#dbeaf4"}
   MouseArea {anchors.fill:parent;onClicked:{wifi.ssid=modelData;wifi.password=""}}
  }
  Text {anchors.centerIn:parent;visible:networks.count===0;text:"点击扫描，选择一个热点";color:"#8897a4";font.pixelSize:19}
 }
 Rectangle {x:629;y:98;width:3;height:Math.max(15,90*90/Math.max(90,networks.contentHeight));color:"#7f919f";visible:networks.contentHeight>90}
 Rectangle {x:16;y:200;width:480;height:43;color:"#1b242c";border.color:"#63717c";radius:3
  Text {x:12;anchors.verticalCenter:parent.verticalCenter;width:456;color:"white";font.pixelSize:21;elide:Text.ElideLeft;text:wifi.password?new Array(wifi.password.length+1).join("•"):(wifi.ssid?"输入所选热点的密码":"请先选择热点")}
 }
 Rectangle {x:508;y:200;width:116;height:43;color:wifi.ssid&&!wifi.busy?"#345e78":"#29343e";radius:3
  Text {anchors.centerIn:parent;text:"连接";color:"white";font.pixelSize:21}
  MouseArea {anchors.fill:parent;enabled:wifi.ssid!==""&&!wifi.busy;onClicked:wifi.action="connect"}
 }
 Grid {id:keyboard;x:16;y:254;columns:10;spacing:6
  Repeater {model:page.keys
   Rectangle {width:55.4;height:44;color:keyMouse.pressed?"#536577":"#303a44";radius:3
    Text {anchors.centerIn:parent;text:modelData;color:"white";font.pixelSize:26}
    MouseArea {id:keyMouse;anchors.fill:parent;enabled:wifi.ssid!==""&&!wifi.busy;onClicked:if(wifi.password.length<63)wifi.password+=modelData}
   }
  }
 }
 Row {x:16;y:409;spacing:8
  Repeater {model:["大小写",keyboardMode===0?"123 / 符号":keyboardMode===1?"更多符号":"ABC","空格","退格"]
   Rectangle {width:146;height:43;color:controlMouse.pressed?"#536577":"#3b4854";radius:3
    Text {anchors.centerIn:parent;text:modelData;color:"white";font.pixelSize:19}
    MouseArea {id:controlMouse;anchors.fill:parent;onClicked:{if(index===0){caps=!caps;keyboardMode=0}else if(index===1)keyboardMode=(keyboardMode+1)%3;else if(index===2){if(wifi.password.length<63)wifi.password+=" "}else wifi.password=wifi.password.slice(0,-1)}}
   }
  }
 }
 Text {x:16;y:460;text:"热点列表可上下滑动 · 临时连接，重启失效";color:"#87949f";font.pixelSize:14}
}
