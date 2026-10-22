#include "about_row.hpp"
#include "qt_stock_profile.hpp"
#include "qt_menu_profile.hpp"
#include "qt_signal_profile.hpp"
#include "qt_binding_profile.hpp"
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQuickItem>
#include <QVariant>
#include <cstdio>
#include <cstdlib>
using namespace x2dui;
static int checks,calls;static void ck(bool v){++checks;if(!v){std::fprintf(stderr,"ABOUT_ROW_FAIL %d\n",checks);std::exit(1);}}
static void format(void*,int call,int id,void**args){ck(call==0&&(id==6||id==7));auto&name=*static_cast<QString*>(args[1]);ck(name=="product_name"||name=="model_number");++calls;*static_cast<QString*>(args[0])=static_cast<QVariant*>(args[2])->toString();}
static const void*cast(const void*meta,const void*object){return static_cast<const QMetaObject*>(meta)->cast(static_cast<const QObject*>(object));}
static QObject*make(QQmlEngine&e,const char*source){QQmlComponent c(&e);c.setData(source,QUrl());auto*o=c.create();if(!o)qWarning()<<c.errors();ck(o);return o;}
int main(int argc,char**argv){QGuiApplication app(argc,argv);QQmlEngine engine;auto stock=qt_stock_profile();auto menu=qt_menu_profile();auto signal=qt_signal_profile();
 for(int fault=0;fault<4;fault++){
  auto*formatter=make(engine,"import QtQuick; QtObject { signal translationValidChanged(bool value) }");void*slot=formatter;FactoryFormatterProfile fp{&slot,&QObject::staticMetaObject,cast,format,stock.string_ctor,stock.string_deallocate};
  auto*model=make(engine,"import QtQuick; QtObject { property string itemName: 'product_name'; property var propValue:'X2D 100C' }");auto*label=make(engine,"import QtQuick; Text { property var sourceModel:null; text: sourceModel ? sourceModel.propValue : 'X2D 100C' }");ck(label->setProperty("sourceModel",QVariant::fromValue(model)));
  auto bp=qt_binding_profile();ck(!AboutRow::create(stock,menu,signal,fp,bp,model,label,false,true));ck(!AboutRow::create(stock,menu,signal,fp,bp,model,label,true,false));
  auto*row=AboutRow::create(stock,menu,signal,fp,bp,model,label,true,true);ck(row);ck(label->property("text")=="X2D 1600C MS");
  ck(model->setProperty("propValue","100C / 100C"));ck(label->property("text")=="1600C MS / 1600C MS");
  ck(model->setProperty("propValue","X2D 100C"));ck(label->property("text")=="X2D 1600C MS");
  if(fault==1){delete model;model=nullptr;ck(!row->refresh());}
  if(fault==2){delete label;label=nullptr;ck(!row->refresh());}
  if(fault==3){delete formatter;formatter=nullptr;ck(!row->refresh());}
  row->close();if(!fault){ck(label->property("text")=="X2D 100C");ck(model->setProperty("propValue","restored factory binding"));ck(label->property("text")=="restored factory binding");}delete model;delete label;delete formatter;
 }
 ck(calls>0);std::printf("ABOUT_ROW_PASS assertions=%d original_proxy_replaced=explicit_readonly_fixture camera_operations=0\n",checks);
}
