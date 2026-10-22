#include "about_attachment.hpp"
#include "qt_stock_profile.hpp"
#include "qt_menu_profile.hpp"
#include "qt_signal_profile.hpp"
#include "qt_binding_profile.hpp"
#include "qt_abi_profile.hpp"
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQuickItem>
#include <QLibrary>
#include <QVariant>
#include <QtQuick/private/qquickanchors_p.h>
#include <QtQuick/private/qquickanchors_p_p.h>
#include <cstdio>
#include <cstdlib>
using namespace x2dui;
static int checks,calls,walks;static AbiArray(*children)(const void*);
static void ck(bool ok){++checks;if(!ok){fprintf(stderr,"ABOUT_ATTACHMENT_FAIL %d\n",checks);std::exit(1);}}
static AbiArray counted(const void*p){++walks;return children(p);}
static void format(void*,int call,int id,void**args){ck(call==0&&(id==6||id==7));++calls;*static_cast<QString*>(args[0])=static_cast<QVariant*>(args[2])->toString();}
static const void*cast(const void*m,const void*o){return static_cast<const QMetaObject*>(m)->cast(static_cast<const QObject*>(o));}
static QObject*make(QQmlEngine&e,const char*s){QQmlComponent c(&e);c.setData(s,QUrl());auto*o=c.create();if(!o)qWarning()<<c.errors();ck(o);return o;}
int main(int argc,char**argv){QGuiApplication app(argc,argv);QQmlEngine engine;auto s=qt_stock_profile();auto m=qt_menu_profile();auto g=qt_signal_profile();auto b=qt_binding_profile();auto a=qt_abi_profile();children=m.children;m.children=counted;
 static_assert(sizeof(AboutAnchorLine)==sizeof(QQuickAnchorLine)&&offsetof(AboutAnchorLine,line)==offsetof(QQuickAnchorLine,anchorLine),"original simple anchor return ABI");
 ck(int(QQuickAnchors::RightAnchor)==2);
 auto right=reinterpret_cast<AboutAnchorLine(*)(const void*)>(QLibrary::resolve("Qt6Quick","_ZNK13QQuickAnchors5rightEv"));ck(right);
 auto*formatter=make(engine,"import QtQuick; QtObject { signal translationValidChanged(bool value) }");void*slot=formatter;FactoryFormatterProfile fp{&slot,&QObject::staticMetaObject,cast,format,s.string_ctor,s.string_deallocate};
 auto*root=make(engine,R"(import QtQuick
 Item { width:800;height:600;property string menuName:'other';objectName:'subDialogLoader';property bool animationSource:false;property bool animatePopupOpen:animationSource
  property QtObject a:QtObject {property string itemName:'product_name';property var propValue:'X2D 100C'}
  property QtObject b:QtObject {property string itemName:'model_number';property var propValue:'100C'}
  Item {id:r1;objectName:'TextDelegate_root';property var model:parent.a;width:800;height:60
   Text {objectName:'title';text:'100C title';anchors.left:parent.left}
   Text {objectName:'value1';text:r1.model.propValue;anchors.right:parent.right}
  }
  Item {id:r2;objectName:'TextDelegate_root';property var model:parent.b;width:800;height:60;y:60
   Text {objectName:'value2';text:r2.model.propValue;anchors.right:parent.right}
  }
  Item {property string menuName:'different';Item {objectName:'TextDelegate_root';property var model:parent.parent.a;Text {objectName:'out-of-scope';text:'100C untouched';anchors.right:parent.right}}}
 })");
 auto*label1=root->findChild<QObject*>("value1");auto*label2=root->findChild<QObject*>("value2");ck(label1&&label2);auto*model=qvariant_cast<QObject*>(root->property("a"));ck(model);
 AboutAttachment attachment(s,m,abi_view_api(&a),g,fp,b,{right,2});ck(attachment.tick(root));ck(attachment.active_rows()==0&&label1->property("text")=="X2D 100C");
 ck(root->setProperty("menuName","aboutMenu"));ck(attachment.tick(root));ck(attachment.active_rows()==2&&label1->property("text")=="X2D 1600C MS"&&label2->property("text")=="1600C MS");ck(root->property("animatePopupOpen").toBool());ck(root->findChild<QObject*>("title")->property("text")=="100C title");ck(root->findChild<QObject*>("out-of-scope")->property("text")=="100C untouched");
 // Consume structural geometry notifications caused by the actual text change.
 app.processEvents();ck(attachment.tick(root));int before=walks;for(int i=0;i<200;i++)ck(attachment.tick(root));ck(walks==before);
 for(int i=0;i<64;i++){ck(model->setProperty("propValue",QString("X2D 100C ")+QString::number(i)));ck(label1->property("text")==QString("X2D 1600C MS ")+QString::number(i));}
 ck(root->setProperty("menuName","other"));ck(attachment.tick(root));ck(!attachment.active_rows());ck(!root->property("animatePopupOpen").toBool());ck(model->setProperty("propValue","factory restored"));ck(label1->property("text")=="factory restored");
 ck(model->setProperty("propValue","X2D 100C"));ck(root->setProperty("menuName","aboutMenu"));ck(attachment.tick(root));ck(attachment.active_rows()==2);ck(model->setProperty("itemName","unrelated"));ck(attachment.tick(root));ck(attachment.active_rows()==1&&label1->property("text")=="X2D 100C");
 ck(model->setProperty("itemName","product_name"));app.processEvents();ck(attachment.tick(root));ck(attachment.active_rows()==2);delete root;ck(!attachment.tick(nullptr));attachment.close();delete formatter;ck(calls>0);printf("ABOUT_ATTACHMENT_PASS assertions=%d realQt=1 explicit_readonly_formatter=1 camera_operations=0\n",checks);
}
