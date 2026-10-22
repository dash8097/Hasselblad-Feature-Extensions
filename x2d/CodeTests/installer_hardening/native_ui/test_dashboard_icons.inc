 // Exercise the production attachment, including cached structural discovery.
 // Model values stand in for camera readback; these are not camera operations.
 {
  QQmlComponent component(&engine);component.setData(R"(import QtQuick
Item {width:1280;height:960
 Item {objectName:"ControlScreen_focusScope"
  property QtObject viewModel:QtObject {id:model;objectName:"DashboardModel";property string mainState:"";property int focusMode:1;property bool focusModeSelectable:true;property string focusModeIcon:"stock-af";property string driveModeIcon:"stock-single";property real driveScale:1.6}
  Item {objectName:"ControlScreen_afControl";property string symbol:model.focusModeIcon}
  Item {objectName:"ControlScreen_driveControl";property string symbol:model.driveModeIcon;property real symbolScale:model.driveScale}
  Loader {objectName:"ControlScreen_popupLoader";active:false}
 }
})",QUrl());
  auto*root=qobject_cast<QQuickItem*>(component.create(engine.rootContext()));C(root);root->setParent(win.contentItem());root->setParentItem(win.contentItem());
  auto*model=root->findChild<QObject*>("DashboardModel");auto*af=root->findChild<QObject*>("ControlScreen_afControl");auto*drive=root->findChild<QObject*>("ControlScreen_driveControl");C(model&&af&&drive);
  FocusAttachment dashboard(api,stock,menu,binding,sig,f,p,{});uint64_t now=100;
  const char*names[]={"AF-S","AF-C","MF"};
  for(int cycle=0;cycle<3;cycle++)for(int mode=1;mode<=3;mode++)for(int enabled=0;enabled<2;enabled++)for(int extension=0;extension<=2;extension++){
   C(model->setProperty("focusMode",mode));C(model->setProperty("focusModeSelectable",bool(enabled)));
   C(dashboard.tick(root,now++,1280,960,true,extension)==0);
   C(af->property("symbol").toString()==QString("image://svg/ic_controlscreen_focus_mode_")+names[mode-1]+(enabled?"":"_disabled"));
   C(drive->property("symbol").toString()==(extension==0?QString("stock-single"):QString("file:///system/etc/x2d-pixel-overclock/")+(extension==1?"multishot.svg":"stack.svg")));
   C(qAbs(drive->property("symbolScale").toDouble()-(extension?1.1:1.6))<0.00001);
  }
  C(dashboard.tick(root,now++,1280,960,true,0)==0);
  for(const char*value:{"stock-continuous","stock-timer","stock-interval","stock-bracket","stock-focus-bracket","stock-single"}){C(model->setProperty("driveModeIcon",value));C(drive->property("symbol").toString()==value);}
  auto scans=dashboard.scans();for(int i=0;i<100;i++)C(dashboard.tick(root,now++,1280,960,true,0)==0);C(dashboard.scans()==scans);
  C(model->setProperty("focusMode",0));C(dashboard.tick(root,now++,1280,960)==0);C(af->property("symbol").toString()=="stock-af");
  C(model->setProperty("focusModeIcon","stock-af-updated"));C(af->property("symbol").toString()=="stock-af-updated");
  C(model->setProperty("focusMode",2));C(dashboard.tick(root,now++,1280,960,true,2)==0);
  dashboard.close();C(af->property("symbol").toString()=="stock-af-updated");C(drive->property("symbol").toString()=="stock-single");C(qAbs(drive->property("symbolScale").toDouble()-1.6)<0.00001);
  C(model->setProperty("driveModeIcon","stock-after-close"));C(drive->property("symbol").toString()=="stock-after-close");delete root;
  // Recreated parameter page uses the current model, not the previous icon.
  root=qobject_cast<QQuickItem*>(component.create(engine.rootContext()));C(root);root->setParent(win.contentItem());root->setParentItem(win.contentItem());
  model=root->findChild<QObject*>("DashboardModel");af=root->findChild<QObject*>("ControlScreen_afControl");drive=root->findChild<QObject*>("ControlScreen_driveControl");
  FocusAttachment reopened(api,stock,menu,binding,sig,f,p,{});C(model->setProperty("focusMode",3));C(reopened.tick(root,100,1280,960,true,1)==0);
  C(af->property("symbol").toString().endsWith("_MF"));C(drive->property("symbol").toString().endsWith("/multishot.svg"));
  // Deleting a borrowed control invalidates its lease and structural cache.
  delete af;C(reopened.tick(root,101,1280,960,true,2)==0);C(drive->property("symbol").toString().endsWith("/stack.svg"));
  reopened.close();delete root;
  printf("DASHBOARD_ICONS_PASS focus=3 selectable=2 extension=3 cycles=3 stock_modes=6 idle_scans=0 restore_binding=1\n");
 }
