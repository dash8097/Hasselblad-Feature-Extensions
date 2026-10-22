#include "native_attachment.hpp"
#include <QtGui/QGuiApplication>
#include <QtQuick/QQuickWindow>
#include <QtCore/QThread>
#include <memory>
static bool guiThread(){return QCoreApplication::instance()&&QCoreApplication::instance()->thread()==QThread::currentThread();}
static QList<QQuickItem*> named(QQuickItem*root,const QString&name){
 QList<QQuickItem*> found;for(auto*child:root->childItems()){if(child->objectName()==name)found.append(child);found.append(named(child,name));}return found;
}
static bool hostValid(QQuickItem*host){
 if(!host||host->thread()!=QThread::currentThread()||!host->window()||host->objectName()!=QStringLiteral("ControlDrawer_root"))return false;
 for(const auto*n:{"MainScreen_grid","menu_loader","MainMenu_swipeArea","ControlDrawer_list"})if(named(host,QString::fromLatin1(n)).size()!=1)return false;
 return true;
}
NativeUiAttachment::~NativeUiAttachment()=default;
NativeUiAttachment::Result NativeUiAttachment::attach(QQuickItem*host,bool enabled,Factory factory){
 if(!guiThread())return WrongThread;
 if(!hostValid(host)||!factory)return InvalidHost;
 auto old=host->findChildren<NativeUiAttachment*>(QString(),Qt::FindDirectChildrenOnly);
 if(!old.isEmpty())return old.size()==1?AlreadyAttached:AmbiguousHost;
 if(!named(host,QStringLiteral("X2dNativeEffectPreferences")).isEmpty())return AmbiguousHost;
 std::unique_ptr<NativeUiAttachment> owner(new NativeUiAttachment(host));
 // Factory must not perform I/O until all ownership checks succeed.
 std::unique_ptr<QQuickItem> component(factory(host,false));
 if(!component||component->thread()!=host->thread()||component->parentItem()!=host)return FactoryFailed;
 auto*effect=qobject_cast<EffectPreferences*>(component.get());
 if(!effect)return FactoryFailed;
 component->setObjectName(QStringLiteral("X2dNativeEffectPreferences"));
 component->setParent(owner.get());owner->item=component.get();
 component.release();effect->setNetworkEnabled(enabled);owner.release();return Attached;
}
NativeUiAttachment::Result NativeUiAttachment::detach(QQuickItem*host){
 if(!guiThread())return WrongThread;
 if(!host||host->thread()!=QThread::currentThread())return InvalidHost;
 auto owners=host->findChildren<NativeUiAttachment*>(QString(),Qt::FindDirectChildrenOnly);
 if(owners.isEmpty())return NotAttached;if(owners.size()!=1)return AmbiguousHost;
 delete owners[0];return Attached;
}
NativeUiAttachment::Result NativeUiAttachment::discoverAndAttach(bool enabled){
 if(!guiThread())return WrongThread;
 QList<QQuickItem*> candidates;
 for(auto*w:QGuiApplication::allWindows())if(auto*q=qobject_cast<QQuickWindow*>(w))candidates.append(named(q->contentItem(),QStringLiteral("ControlDrawer_root")));
 if(candidates.isEmpty())return InvalidHost;if(candidates.size()!=1)return AmbiguousHost;
 return attach(candidates[0],enabled);
}
extern "C" Q_DECL_EXPORT int x2d_native_attach_v1(QQuickItem*p,bool enabled){try{return NativeUiAttachment::attach(p,enabled);}catch(...){return NativeUiAttachment::FactoryFailed;}}
extern "C" Q_DECL_EXPORT int x2d_native_detach_v1(QQuickItem*p){try{return NativeUiAttachment::detach(p);}catch(...){return NativeUiAttachment::FactoryFailed;}}
extern "C" Q_DECL_EXPORT int x2d_native_discover_v1(bool enabled){try{return NativeUiAttachment::discoverAndAttach(enabled);}catch(...){return NativeUiAttachment::FactoryFailed;}}
