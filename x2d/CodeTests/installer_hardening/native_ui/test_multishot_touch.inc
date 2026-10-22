// Real window pointer delivery (QtTest); no physical camera input is sent.
{
 auto tap=[&](QQuickItem*item){CK(item&&item->isVisible());auto pos=item->mapToScene(QPointF(item->width()/2,item->height()/2)).toPoint();QTest::mouseClick(&window,Qt::LeftButton,Qt::NoModifier,pos);app.processEvents();};
 for(bool stack:{true,false}){
  PixelController c;ready(c);PixelHost h(c);CK(h.select(stack?ExtensionMode::Stack:ExtensionMode::Pixel));CK(h.stack_options(0,7,false,0,16));
  DriveSettingsAttachment attachment(stock,menu,&abi,signals,binding,keys,keymap,focus,profile,c,h);auto*scene=fixture(engine,window.contentItem());
  auto*model=static_cast<QObject*>(stock_get_object(stock,scene->findChild<QQuickItem*>("ControlScreen_focusScope"),"viewModel"));
  CK(attachment.tick(scene,10,{},ds,ss,1024,768));QTest::qWait(20);
  auto capture=[&](const QString&name){window.requestUpdate();QTest::qWait(30);auto frame=window.grabWindow();CK(!frame.isNull()&&frame.save(QDir(QString::fromLocal8Bit(argv[2])).filePath(name)));};
  capture(stack?"stack-summary.png":"pixel-summary.png");
  QTest::mouseClick(&window,Qt::LeftButton,Qt::NoModifier,QPoint(800,300));app.processEvents();CK(attachment.tick(scene,11,{},ds,ss,1024,768));
  auto*settings=scene->findChild<QQuickItem*>("X2dNativeMultishotSettings");CK(settings&&settings->isVisible());
  tap(settings->findChild<QQuickItem*>("X2dMultishotSettings_touch0"));CK(settings->findChild<QQuickItem*>("X2dSettingsChoice3")->isVisible());
  capture(stack?"stack-selector.png":"pixel-selector.png");
  tap(settings->findChild<QQuickItem*>("X2dSettingsChoice4"));CK(attachment.tick(scene,12,{},ds,ss,1024,768));
  CK(stack?h.state().stackMethod==1:c.state().resolution==16);CK(!settings->findChild<QQuickItem*>("X2dSettingsChoice3")->isVisible());
  capture(stack?"stack-settings.png":"pixel-settings.png");
  // EVF: touch is ignored while physical navigation still works.
  CK(model->setProperty("screenEVF",true));CK(attachment.tick(scene,13,{},ds,ss,1024,768));
  tap(settings->findChild<QQuickItem*>("X2dMultishotSettings_touch0"));CK(!settings->findChild<QQuickItem*>("X2dSettingsChoice3")->isVisible());
  CK(key(Qt::Key_Return));CK(settings->findChild<QQuickItem*>("X2dSettingsChoice3")->isVisible());CK(key(Qt::Key_Escape));
  CK(key(Qt::Key_Escape));CK(attachment.tick(scene,14,{},ds,ss,1024,768));CK(!scene->findChild<QQuickItem*>("X2dNativeMultishotSettings"));
  CK(model->setProperty("screenEVF",false));CK(attachment.tick(scene,15,{},ds,ss,1024,768));
  QTest::mouseClick(&window,Qt::LeftButton,Qt::NoModifier,QPoint(800,300));app.processEvents();CK(attachment.tick(scene,16,{},ds,ss,1024,768));CK(scene->findChild<QQuickItem*>("X2dNativeMultishotSettings"));
  // Parent teardown with a selector open, then fresh ownership on reentry.
  CK(key(Qt::Key_Return));delete scene;CK(!attachment.tick(nullptr,17,{},ds,ss,1024,768));
  scene=fixture(engine,window.contentItem());CK(attachment.tick(scene,18,{},ds,ss,1024,768));CK(key(Qt::Key_F2));CK(attachment.tick(scene,19,{},ds,ss,1024,768));CK(scene->findChild<QQuickItem*>("X2dNativeMultishotSettings"));
  attachment.close();delete scene;
 }
 puts("MULTISHOT_TOUCH_PASS actual_window_pointer_delivery=1 evf_touch_blocked=1 keyboard_reentry=1 open_selector_parent_teardown=1 screenshots=6 camera_operations=0");
}
