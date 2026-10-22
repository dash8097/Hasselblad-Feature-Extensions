#include "album_adapter.hpp"
#include "pixel_file_io.h"
#include "qt_stock_profile.hpp"
#include "qt_menu_profile.hpp"
#include <QGuiApplication>
#include <QQuickItem>
#include <QQmlComponent>
#include <QQmlEngine>
#include <cstdio>
#include <cstdlib>
using namespace x2dui;
extern "C" int x2d_pixel_io_submit(X2dPixelIo*,const X2dPixelIoRequest*){std::abort();}
extern "C" int x2d_pixel_io_poll(X2dPixelIo*,X2dPixelIoReply*){std::abort();}
extern "C" int x2d_pixel_io_cancel(X2dPixelIo*,uint64_t){std::abort();}
static unsigned checks,refreshes,enters,loads;static QString captured;
static void check(bool b,const char*n){++checks;if(!b){fprintf(stderr,"ALBUM_ADAPTER_FAIL %s\n",n);exit(1);}}
int main(int argc,char**argv){QGuiApplication app(argc,argv);QQmlEngine engine;QQmlComponent c(&engine);c.setData("import QtQuick\nItem {Item {objectName: \"BrowseView_root\";property QtObject viewModel:QtObject {property bool browseViewActive:false;property QtObject currentItemModel:QtObject{};property QtObject model:QtObject {property int listSize:1}}}}",QUrl());auto*root=static_cast<QQuickItem*>(c.create());check(root,"fixture-root");QObject content;content.setProperty("path",QString("/ssd/100HASBL"));content.setProperty("isBrowsing",false);void*slot=&content;AlbumProfile p{&slot,&QObject::staticMetaObject,[](const void*m,const void*o)->const void*{return static_cast<const QMetaObject*>(m)->cast(static_cast<const QObject*>(o));},[](void*,const AbiString&s){++refreshes;captured=*reinterpret_cast<const QString*>(&s);},[](void*,const AbiString&s){++enters;captured=*reinterpret_cast<const QString*>(&s);},[](void*){++loads;}};AlbumAdapter adapter(p,qt_stock_profile(),qt_menu_profile());AlbumContext context;check(adapter.snapshot(root,false,context)&&context.path=="/ssd/100HASBL"&&!context.viewingPhoto,"current-original-snapshot");
 for(unsigned n=0;n<128;n++){PixelHostAction a{PixelHostActionKind::RefreshAlbumSamePath,"/ssd/100HASBL"};check(adapter.dispatch(a,root,false),"same-path-refresh");check(captured=="/ssd/100HASBL","owned-string-retained-after-return");}check(refreshes==128&&!enters&&!loads,"only-signal-called");
 auto*browse=root->findChild<QObject*>("BrowseView_root");auto*vm=browse->property("viewModel").value<QObject*>();vm->setProperty("browseViewActive",true);PixelHostAction enter{PixelHostActionKind::EnterAlbum,"/ssd/101HASBL"};check(adapter.snapshot(root,false,context)&&context.viewingPhoto&&!adapter.dispatch(enter,root,false),"viewing-photo-defers-refresh");vm->setProperty("browseViewActive",false);check(!adapter.dispatch(enter,root,true),"storage-browse-defers-refresh");content.setProperty("isBrowsing",true);check(!adapter.dispatch(enter,root,false),"content-browse-defers-refresh");content.setProperty("isBrowsing",false);check(adapter.dispatch(enter,root,false)&&enters==1&&captured=="/ssd/101HASBL","enter-native-album");check(adapter.dispatch({PixelHostActionKind::UpdateAlbumLoading,{}},root,false)&&loads==1,"original-loading-refresh");check(!adapter.dispatch({PixelHostActionKind::SetStockSingle,{}},root,false),"no-camera-setter-route");check(!adapter.dispatch({PixelHostActionKind::EnterAlbum,"/ssd/../data"},root,false),"invalid-album-rejected");check(!adapter.dispatch({PixelHostActionKind::RefreshAlbumSamePath,"/ssd/101HASBL"},root,false),"stale-path-refresh-rejected");slot=nullptr;check(!adapter.snapshot(root,false,context)&&!adapter.dispatch(enter,root,false),"no-singleton-create");delete root;printf("NATIVE_ALBUM_ADAPTER_PASS assertions=%u original_calls=recorders camera_requests=0\n",checks);}
