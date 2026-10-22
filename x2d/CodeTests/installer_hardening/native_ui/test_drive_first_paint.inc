// A synchronous stock Loader must be replaced in its loaded delivery,
 // without waiting for any later 50-ms service tick or processing events.
 {
  QQmlComponent fc(&engine);fc.setData(R"(import QtQuick
Item {width:1024;height:768
 Item {id:control;objectName:"ControlScreen_focusScope";width:1024;height:768
 property QtObject viewModel:QtObject {property string mainState:"";property int driveMode:0;property int expMode:0;property bool lensHasMicroFocusAdjust:true;property bool screenEVF:false;property string aperture:"f/5";property string shutterSpeed:"1/250";property string isoAsString:"200"}
 Loader {objectName:"ControlScreen_popupLoader";active:false;asynchronous:false
 sourceComponent:Component {Item {objectName:"PopoverDriveMode_root";width:1024;height:768;property bool initialized:false
 Item {id:grid;objectName:"DriveModeGrid_root";property int currentIndex:0;signal openDialog(string settingsList);onOpenDialog:(settingsList)=>{swipe.show=true;swipe.settingsList=settingsList}}
 Item {id:swipe;objectName:"PopoverDriveMode_swipe_area";property bool show:false;property string settingsList:""}
 Item {objectName:"fixtureSummary";property string settingsList:"";property string description:"fixture";property string headerText:"fixture";Item {property string text:"synthetic label";property string valueText:"synthetic value"}}
 }}
 onLoaded: item.initialized=true
 }
 }
})",QUrl());auto*scene=qobject_cast<QQuickItem*>(fc.create(engine.rootContext()));CK(scene);scene->setParent(window.contentItem());scene->setParentItem(window.contentItem());
  auto*control=scene->findChild<QQuickItem*>("ControlScreen_focusScope");auto*model=static_cast<QObject*>(stock_get_object(stock,control,"viewModel"));auto*loader=scene->findChild<QObject*>("ControlScreen_popupLoader");CK(model&&loader);
  PixelController c;ready(c);PixelHost h(c);DriveSettingsAttachment attachment(stock,menu,&abi,signals,binding,keys,keymap,focus,profile,c,h);
  CK(!attachment.tick(scene,10,{},ds,ss,1024,768));
  for(int j=0;j<20;++j){
   CK(model->setProperty("mainState","drive_mode"));CK(loader->setProperty("active",true));
   auto*original=static_cast<QQuickItem*>(stock_get_object(stock,loader,"item"));CK(original&&original->property("initialized").toBool());
   CK(attachment.attached()&&!original->isVisible()&&!original->isEnabled());
   CK(scene->findChildren<QQuickItem*>("X2dNativeDriveMode").size()==1);
   CK(model->setProperty("mainState",""));CK(!attachment.tick(scene,20+j,{},ds,ss,1024,768));CK(loader->setProperty("active",false));
  }
  // Disconnect before destruction: a later stock-only open remains untouched.
  attachment.close();CK(model->setProperty("mainState","drive_mode"));CK(loader->setProperty("active",true));auto*original=static_cast<QQuickItem*>(stock_get_object(stock,loader,"item"));CK(original&&original->isVisible()&&original->isEnabled());delete scene;
 }
