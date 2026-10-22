// Included in the real Qt drive attachment harness. Camera properties are
// explicit fixtures; this checks UI preservation, not physical AF/exposure.
{
 const char*shutters[]={"1/4000","1/1000","1/125","0.4","1","30"};
 const char*apertures[]={"f/2.5","f/4","f/8","f/16"};
 const char*isos[]={"64","100","400","3200","25600"};
 unsigned scenarios=0,reentries=0;
 {PixelController c;ready(c);PixelHost h(c);CK(!h.stack_options(3,7,false,0,16));}
 for(int variant=0;variant<6;variant++)for(int prior=0;prior<6;prior++)for(int bits:{14,16})for(int af=0;af<3;af++)for(int face=0;face<3;face++){
  PixelController c;ready(c);PixelHost h(c);bool stack=variant<3;
  if(stack)CK(h.stack_options(variant,7,false,0,16));else CK(c.set_option("pixelResolution",variant==3?6:variant==4?16:25));
  CK(h.select(stack?ExtensionMode::Stack:ExtensionMode::Pixel));
  DriveSettingsAttachment attachment(stock,menu,&abi,signals,binding,keys,keymap,focus,profile,c,h);
  auto*scene=fixture(engine,window.contentItem());auto*control=scene->findChild<QQuickItem*>("ControlScreen_focusScope");
  auto*model=static_cast<QObject*>(stock_get_object(stock,control,"viewModel"));auto*original=scene->findChild<QQuickItem*>("PopoverDriveMode_root");
  CK(model->setProperty("driveMode",prior));model->setProperty("bitDepth",bits);model->setProperty("focusMode",af);model->setProperty("faceMode",face);
  uint64_t now=20;QString shutter,aperture,iso;
  for(int cycle=0;cycle<3;cycle++){
   shutter=QString::fromLatin1(shutters[(scenarios+cycle)%6]);aperture=QString::fromLatin1(apertures[(scenarios/3+cycle)%4]);iso=QString::fromLatin1(isos[(scenarios/7+cycle)%5]);
   CK(model->setProperty("shutterSpeed",shutter));CK(model->setProperty("aperture",aperture));CK(model->setProperty("isoAsString",iso));
   CK(model->setProperty("mainState",QString("drive_mode")));CK(attachment.tick(scene,now++,{},ds,ss,1024,768));
   auto*popup=scene->findChild<QQuickItem*>("X2dNativeDriveMode");CK(popup&&popup->isVisible());
   CK(!visibleText(popup,u"光圈")&&!visibleText(popup,u"快门")&&!visibleText(popup,u"ISO")&&!visibleText(popup,u"拍摄 ISO")&&!visibleText(popup,u"拍摄快门"));
   CK(key(Qt::Key_F2));CK(attachment.tick(scene,now++,{},ds,ss,1024,768));
   auto*settings=scene->findChild<QQuickItem*>("X2dNativeMultishotSettings");CK(settings&&settings->isVisible()&&window.activeFocusItem()==settings);
   if(stack){CK(settings->findChild<QObject*>("X2dMultishotSettings_value5")->property("valueText").toString()==aperture);CK(settings->findChild<QObject*>("X2dMultishotSettings_value6")->property("valueText").toString()==shutter);CK(settings->findChild<QObject*>("X2dMultishotSettings_value7")->property("valueText").toString()==iso);}
   CK(key(Qt::Key_Return));CK(key(Qt::Key_Escape)); // Open and cancel selector.
   // Reproduce the user sequence: the old handler dies between press/release.
   QKeyEvent press(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);QGuiApplication::sendEvent(&window,&press);CK(press.isAccepted());
   CK(attachment.tick(scene,now++,{},ds,ss,1024,768));CK(!scene->findChild<QQuickItem*>("X2dNativeMultishotSettings"));
   QKeyEvent release(QEvent::KeyRelease,Qt::Key_Escape,Qt::NoModifier);QGuiApplication::sendEvent(&window,&release);
   CK(popup->findChildren<QQuickKeysAttached*>(QString(),Qt::FindDirectChildrenOnly).size()==1);
   CK(model->property("driveMode").toInt()==prior&&model->property("bitDepth").toInt()==bits&&model->property("focusMode").toInt()==af&&model->property("faceMode").toInt()==face);
   CK(model->property("shutterSpeed").toString()==shutter&&model->property("aperture").toString()==aperture&&model->property("isoAsString").toString()==iso);
   DriveSessionIntent intent;CK(!attachment.take_intent(intent,now++));CK(!c.state().applied&&!c.state().busy); // Navigation never arms/captures.
   CK(model->setProperty("mainState",QString("shutter_speed")));CK(!attachment.tick(scene,now++,{},ds,ss,1024,768));CK(original->isVisible()&&original->isEnabled());++reentries;
  }
  attachment.close();delete scene;++scenarios;
 }
 // M -> A/S/P/FullAuto/unknown while already inside, then M again, including
 // changes between the last UI refresh and the user's press.
 for(bool stack:{false,true})for(int mode:{1,2,3,4,255}){
  PixelController c;ready(c);PixelHost h(c);CK(h.select(stack?ExtensionMode::Stack:ExtensionMode::Pixel));
  DriveSettingsAttachment attachment(stock,menu,&abi,signals,binding,keys,keymap,focus,profile,c,h);auto*scene=fixture(engine,window.contentItem());
  auto*model=static_cast<QObject*>(stock_get_object(stock,scene->findChild<QQuickItem*>("ControlScreen_focusScope"),"viewModel"));
  CK(attachment.tick(scene,20,{},ds,ss,1024,768));CK(key(Qt::Key_F2));CK(attachment.tick(scene,21,{},ds,ss,1024,768));CK(scene->findChild<QQuickItem*>("X2dNativeMultishotSettings"));
  CK(model->setProperty("expMode",mode));CK(attachment.tick(scene,22,{},ds,ss,1024,768));CK(!scene->findChild<QQuickItem*>("X2dNativeMultishotSettings"));
  auto*icon=scene->findChild<QObject*>(stack?"NativeDriveIcon7":"NativeDriveIcon6");CK(icon&&!icon->property("selectable").toBool());
  key(Qt::Key_F2);CK(attachment.tick(scene,23,{},ds,ss,1024,768));CK(!scene->findChild<QQuickItem*>("X2dNativeMultishotSettings")&&!c.state().applied);
  CK(model->setProperty("expMode",0));CK(attachment.tick(scene,24,{},ds,ss,1024,768));CK(icon->property("selectable").toBool());
  CK(key(Qt::Key_F2));CK(attachment.tick(scene,25,{},ds,ss,1024,768));CK(scene->findChild<QQuickItem*>("X2dNativeMultishotSettings"));
  CK(key(Qt::Key_Escape));CK(attachment.tick(scene,26,{},ds,ss,1024,768));CK(model->setProperty("expMode",mode)); // No refresh before press.
  key(Qt::Key_Return);CK(!c.state().applied);attachment.close();delete scene;
 }
 printf("MULTISHOT_REENTRY_MATRIX_PASS scenarios=%u reentries=%u variants=6 removed_iso_rejected=1 previous_drive_modes=6 depths=2 focus_modes=3 recognition_modes=3 shutters=6 apertures=4 isos=5 camera_operations=0\n",scenarios,reentries);
}
