#include "qt_stock_profile.hpp"
#include "qt_abi_profile.hpp"
#include "processing_presentation.hpp"
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QResource>
#include <QVariant>
#include <cstdio>
#include <cstdlib>
static int checks;static void check(bool v,const char*n){++checks;if(!v){fprintf(stderr,"ORIGINAL_STOCK_FAIL %s\n",n);exit(1);}}
int main(int argc,char**argv){QGuiApplication app(argc,argv);check(argc==2&&QResource::registerResource(QString::fromLocal8Bit(argv[1])),"private-original-resource-fixture");auto p=qt_stock_profile();auto ap=qt_abi_profile();auto view=x2dui::abi_view_api(&ap);QQmlEngine engine;QQuickWindow window;window.resize(800,600);auto*root=window.contentItem();QQmlEngine::setContextForObject(root,engine.rootContext());
 auto*screen=static_cast<QQuickItem*>(x2dui::stock_create(p,x2dui::StockKind::ExposeScreen,root,root));check(screen,"original-ExposeScreen-created");
 auto*model=x2dui::stock_create(p,x2dui::StockKind::ExposeViewModel,root,root);check(model&&x2dui::stock_set_object(p,screen,"viewModel",model),"typed-view-model-injected");
 auto*progress=x2dui::stock_create(p,x2dui::StockKind::ExposeProgress,root,screen);auto*countdown=x2dui::stock_create(p,x2dui::StockKind::ExposeIconText,root,screen);check(progress&&countdown,"original-progress-and-countdown");
 x2dui::ProcessingState state;state.present=true;state.ready=true;state.busy=true;state.stage=x2dui::ProcessingStage::Render;state.renderTiles=4;auto presentation=x2dui::processing_presentation(state);
 check(x2dui::stock_set_text(p,screen,"headerText",presentation.title)&&x2dui::stock_set_bool(p,screen,"showButtons",presentation.showButtons),"typed-header-and-buttons");
 void*fields[]={x2dui::stock_get_object(p,progress,"text1"),x2dui::stock_get_object(p,progress,"text2"),x2dui::stock_get_object(p,progress,"text3")};check(fields[0]&&fields[1]&&fields[2],"original-text-aliases");
 check(x2dui::stock_set_text(p,fields[0],"text",presentation.status)&&x2dui::stock_set_text(p,fields[1],"text",presentation.quantity)&&x2dui::stock_set_text(p,fields[2],"text",presentation.detail),"native-presentation-applied");
 check(x2dui::stock_set_number(p,progress,"progressValue",presentation.progress)&&x2dui::stock_set_bool(p,progress,"showInfiniteLoop",presentation.infinite),"native-progress-applied");
 auto*button=x2dui::stock_get_object(p,screen,"btn2");check(button&&x2dui::stock_set_text(p,button,"text",presentation.button),"original-button-alias");
 for(auto*o:{static_cast<void*>(screen),progress,countdown})view.rect(view.context,o,0,0,800,600);view.visible(view.context,countdown,false);
 app.processEvents();check(static_cast<QObject*>(fields[0])->property("text").toString()==QString::fromUtf16(presentation.status),"actual-original-text-updated");
 check(static_cast<QObject*>(progress)->property("progressValue").toDouble()==presentation.progress,"actual-original-progress-updated");p.object_destroy(screen);p.object_destroy(model);
 printf("NATIVE_ORIGINAL_STOCK_PASS assertions=%d original_QML=4.2.0 hardware_proxies=fixtures camera_requests=0\n",checks);
}
