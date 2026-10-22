 // Real Loader initializes the factory list in onLoaded. No later tick,
 // timer, processEvents, or mocked connection is used to make attach succeed.
 {
  QQmlComponent fc(&engine);fc.setData(R"(import QtQuick
Item {width:1280;height:960
 Item {id:control;objectName:"ControlScreen_focusScope";width:1280;height:960
 property QtObject viewModel:QtObject {id:model;property string mainState:"";property bool focusModeSelectable:true}
 QtObject {id:one;property int focusMode:1;property bool valid:true}
 QtObject {id:three;property int focusMode:3;property bool valid:true}
 Loader {objectName:"ControlScreen_popupLoader";active:false;asynchronous:false
 sourceComponent:Component {Item {objectName:"PopoverFocusMode_root";width:1280;height:960;property string heading:"对焦模式";property int focusMode:0;property list<QtObject> focusModeModel:[];signal setFocusMode(int mode);onSetFocusMode:(mode)=>focusMode=mode}}
 onLoaded: {item.focusMode=3;item.focusModeModel=[one,three]}
 }
 }
})",QUrl());auto*root=qobject_cast<QQuickItem*>(fc.create(engine.rootContext()));C(root);root->setParent(win.contentItem());root->setParentItem(win.contentItem());auto*control=root->findChild<QQuickItem*>("ControlScreen_focusScope");auto*model=static_cast<QObject*>(stock_get_object(stock,control,"viewModel"));auto*loader=root->findChild<QObject*>("ControlScreen_popupLoader");C(model&&loader);
  FocusAttachment first(api,stock,menu,binding,sig,f,p,{});C(first.tick(root,100,1280,960)==0&&!first.attached());
  for(int j=0;j<20;++j){
   C(model->setProperty("mainState","focus_mode"));C(loader->setProperty("active",true));auto*original=static_cast<QQuickItem*>(stock_get_object(stock,loader,"item"));C(original&&original->property("focusMode").toInt()==3);
   C(first.attached()&&!original->isVisible()&&!original->isEnabled());C(root->findChildren<QQuickItem*>("X2dNativeFocusPopup").size()==1);
   C(model->setProperty("mainState",""));C(first.tick(root,200+j,1280,960)==0&&!first.attached());C(loader->setProperty("active",false));
  }
  first.close();C(model->setProperty("mainState","focus_mode"));C(loader->setProperty("active",true));auto*original=static_cast<QQuickItem*>(stock_get_object(stock,loader,"item"));C(original&&original->isVisible()&&original->isEnabled());delete root;
 }
