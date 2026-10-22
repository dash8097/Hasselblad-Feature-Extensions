#include "native_attachment.hpp"
#include <QtGui/QGuiApplication>
#include <QtQuick/QQuickWindow>
#include <cstdio>
class Item:public QQuickItem {public:using QQuickItem::QQuickItem;~Item()override=default;};
int main(int argc,char**argv){QGuiApplication app(argc,argv);QQuickWindow w;w.show();app.processEvents();auto*p=new Item(w.contentItem());p->setObjectName("ControlDrawer_root");for(const char*n:{"MainScreen_grid","menu_loader","MainMenu_swipeArea","ControlDrawer_list"}){auto*c=new Item(p);c->setObjectName(n);}puts("BEFORE");fflush(stdout);new EffectPreferences(p);printf("attach=manual\n");fflush(stdout);QPointer<EffectPreferences> guard=p->findChild<EffectPreferences*>();delete p;puts("DELETED");fflush(stdout);return guard?1:0;}
