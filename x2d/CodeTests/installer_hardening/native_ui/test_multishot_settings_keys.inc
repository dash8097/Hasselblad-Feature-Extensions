// Stock grid wheel Left/Right mapping must become vertical only in settings.
// Deliver real Qt window key events, and assert the owning controller values.
{
 for(bool stack:{false,true}){
  PixelController c;ready(c);CK(c.set_option("initialDelay",2));
  PixelHost h(c);CK(h.select(stack?ExtensionMode::Stack:ExtensionMode::Pixel));CK(h.stack_options(0,7,false,2,16));
  DriveSettingsAttachment attachment(stock,menu,&abi,signals,binding,keys,keymap,focus,profile,c,h);
  auto*scene=fixture(engine,window.contentItem());CK(attachment.tick(scene,10,{},ds,ss,1024,768));QTest::qWait(20);
  // Select remains outside the original grid's accept mapping.
  CK(!key(Qt::Key_D));CK(!scene->findChild<QQuickItem*>("X2dNativeMultishotSettings"));
  QTest::mouseClick(&window,Qt::LeftButton,Qt::NoModifier,QPoint(800,300));app.processEvents();CK(attachment.tick(scene,11,{},ds,ss,1024,768));
  auto*settings=scene->findChild<QQuickItem*>("X2dNativeMultishotSettings");CK(settings&&settings->isVisible());
  auto*row=settings->findChild<QQuickItem*>(stack?"X2dMultishotSettings_touch4":"X2dMultishotSettings_touch1");CK(row&&row->isVisible());
  auto pos=row->mapToScene(QPointF(row->width()/2,row->height()/2)).toPoint();QTest::mouseClick(&window,Qt::LeftButton,Qt::NoModifier,pos);app.processEvents();
  CK(settings->findChild<QQuickItem*>("X2dSettingsChoice3")->isVisible());
  CK(key(Qt::Key_Right));CK(key(Qt::Key_D));CK(attachment.tick(scene,12,{},ds,ss,1024,768));
  CK(stack?h.state().initialDelay==3:c.state().initialDelay==3);
  CK(!settings->findChild<QQuickItem*>("X2dSettingsChoice3")->isVisible());
  CK(key(Qt::Key_D));CK(key(Qt::Key_Left));CK(key(Qt::Key_Escape));CK(attachment.tick(scene,13,{},ds,ss,1024,768));
  CK(stack?h.state().initialDelay==3:c.state().initialDelay==3);
  CK(key(Qt::Key_D));CK(key(Qt::Key_Left));CK(key(Qt::Key_D));CK(attachment.tick(scene,14,{},ds,ss,1024,768));
  CK(stack?h.state().initialDelay==2:c.state().initialDelay==2);
  CK(key(Qt::Key_Escape));CK(attachment.tick(scene,15,{},ds,ss,1024,768));
  CK(!scene->findChild<QQuickItem*>("X2dNativeMultishotSettings"));
  attachment.close();delete scene;
 }
 puts("MULTISHOT_WHEEL_SELECT_PASS pixel_and_stack_delay=2 wheel_commit_cancel=1 grid_select_unchanged=1 actual_window_keys=1 camera_operations=0");
}
