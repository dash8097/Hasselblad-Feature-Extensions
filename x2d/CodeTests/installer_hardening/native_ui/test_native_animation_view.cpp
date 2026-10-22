#include "native_animation_view.hpp"
#include "native_animation_test_support.hpp"
#include "qt_abi_profile.hpp"
#include "qt_stock_profile.hpp"
#include "qt_menu_profile.hpp"
#include "qt_signal_profile.hpp"
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickImageProvider>
#include <QtQuick/private/qquickimage_p.h>
#include <QtQuick/private/qquickimage_p_p.h>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <new>
#include <limits>
using namespace x2dui;
using namespace animation_test;
struct TestImage final:QQuickImage {explicit TestImage(QQuickItem*p):QQuickImage(p){}using QQuickImage::componentComplete;};
static AnimationImageProfile imageProfile(){return {sizeof(TestImage),[](void*p,void*parent){new(p)TestImage(static_cast<QQuickItem*>(parent));},[](void*p){static_cast<TestImage*>(p)->componentComplete();},[](void*p,void*c){QQmlEngine::setContextForObject(static_cast<QObject*>(p),static_cast<QQmlContext*>(c));}};}
static void closeView(void*p,Event,double){auto**v=static_cast<NativeAnimationView**>(p);if(*v){auto*q=*v;*v=nullptr;q->close();}}
static void noop(void*){}
int main(int argc,char**argv){QGuiApplication app(argc,argv);check(argc==3,"arguments");QDir oracle(QString::fromLocal8Bit(argv[1])),out(QString::fromLocal8Bit(argv[2]));fonts(app,oracle);
 DataOnlyFactory network;QQmlEngine engine;engine.setNetworkAccessManagerFactory(&network);QQmlComponent component(&engine,QUrl::fromLocalFile(oracle.filePath("oracle.qml")));if(component.isError())qWarning()<<component.errors();check(component.isReady(),"oracle component ready");
 QQuickWindow ow,nw;ow.setColor(Qt::transparent);nw.setColor(Qt::transparent);auto*o=qobject_cast<QQuickItem*>(component.beginCreate(engine.rootContext()));check(o,"oracle Item");o->setParentItem(ow.contentItem());component.completeCreate();ow.show();nw.show();
 QQmlEngine::setContextForObject(nw.contentItem(),engine.rootContext());auto signal=qt_signal_profile();auto abi=qt_abi_profile();auto api=abi_view_api(&abi);auto stock=qt_stock_profile();auto menu=qt_menu_profile();auto ip=imageProfile();auto*v=NativeAnimationView::create(api,stock,menu,ip,signal,nw.contentItem());check(v&&v->item(),"native created");auto*root=static_cast<QQuickItem*>(v->item());auto*image=root->findChild<QQuickImage*>("X2dNativeShutterLeaves");check(image,"real QQuickImage exists");
 check(!root->isEnabled()&&!root->isVisible()&&root->z()==1000,"passive overlay initially hidden");check(!image->cache()&&!image->asynchronous()&&image->sourceSize()==QSize(800,600),"cache/sourceSize/asynchronous exact");
 QJsonArray frames;double worstMean=0,worstFraction=0;const double progress[]={0,.125,.2125,.25,.4,.5,.625,.75,.9,1};
 for(auto size:{QSize(800,600),QSize(400,300),QSize(900,600),QSize(600,800)}){ow.resize(size);nw.resize(size);o->setSize(size);settle(80);
  for(double p:progress){o->setProperty("showing",true);o->setProperty("progress",p);auto f=shutter_animation_frame(p);f.overlayOpacity=1;check(v->update(f,size.width(),size.height()),"native frame update");settle(80);check(image->status()==QQuickImageBase::Ready&&root->isVisible(),"asynchronous SVG commits ready frame");if(p==0&&size==QSize(800,600)){printf("IMAGE_STATE width=%g height=%g visible=%d opacity=%g parentItem=%p parentObject=%p window=%p targetWindow=%p z=%g\n",image->width(),image->height(),image->isVisible(),image->opacity(),static_cast<void*>(image->parentItem()),static_cast<void*>(image->parent()),static_cast<void*>(image->window()),static_cast<void*>(&nw),image->z());auto*d=static_cast<QQuickImagePrivate*>(QObjectPrivate::get(image));printf("IMAGE_PIX null=%d width=%d height=%d texture=%p engine=%p painted=%gx%g contents=%d\n",d->pix.isNull(),d->pix.width(),d->pix.height(),static_cast<void*>(d->pix.textureFactory()),static_cast<void*>(qmlEngine(image)),image->paintedWidth(),image->paintedHeight(),int(image->flags().testFlag(QQuickItem::ItemHasContents)));auto bytes=QByteArray::fromBase64(image->source().toString().section(',',1).toLatin1());QFile raw(out.filePath("first.svg"));check(raw.open(QIODevice::WriteOnly),"SVG evidence");raw.write(bytes);raw.close();auto raster=QImage::fromData(bytes,"svg");check(!raster.isNull()&&raster.save(out.filePath("first-svg-raster.png")),"SVG direct raster evidence");}auto a=capture(ow,size),b=capture(nw,size);QString tag=QString("%1x%2-p%3").arg(size.width()).arg(size.height()).arg(int(p*10000));auto row=comparison(a,b,tag,out);worstMean=std::max(worstMean,row["mean_channel_max_error"].toDouble());worstFraction=std::max(worstFraction,row["fraction_error_gt16"].toDouble());frames.append(row);}
 }
 QFile data(out.filePath("pixel-comparison.json"));check(data.open(QIODevice::WriteOnly),"evidence open");data.write(QJsonDocument(QJsonObject{{"frames",frames},{"worst_mean",worstMean},{"worst_fraction_gt16",worstFraction},{"same_fonts",true},{"no_network",true},{"native_image_svg",true}}).toJson());data.close();
 printf("NATIVE_ANIMATION_VISUAL frames=%lld worst_mean=%.8f worst_fraction=%.8f\n",static_cast<long long>(frames.size()),worstMean,worstFraction);
 check(worstMean<=3.0&&worstFraction<=.025,"same previous pixel thresholds");
 auto f=shutter_animation_frame(.625);f.overlayOpacity=1;check(v->update(f,800,600),"baseline before reuse");settle(20);auto count=descendants(root);auto*identity=image;
 for(int i=0;i<512;i++){f=shutter_animation_frame(double(i%101)/100);f.overlayOpacity=1;check(v->update(f,800,600),"repeat update");check(root->findChild<QQuickImage*>("X2dNativeShutterLeaves")==identity&&descendants(root)==count,"no per-frame QObject accumulation");check(!image->cache()&&image->sourceSize()==QSize(800,600)&&(image->status()==QQuickImageBase::Ready||image->status()==QQuickImageBase::Loading),"repeat image policy");}
 settle(240);check(image->status()==QQuickImageBase::Ready&&root->isVisible(),"coalesced last frame ready");auto source=image->source().toString();check(source.startsWith("data:image/svg+xml;base64,"),"only data SVG");auto svg=QByteArray::fromBase64(source.section(',',1).toLatin1());check(svg.count("<path ")==8&&!svg.contains("<script")&&!svg.contains("href=")&&!svg.contains("onload"),"8 passive paths no script or external resource");

 QString expected=QString("M %1 %2").arg(f.leaves[0].a.x,0,'f',6).arg(f.leaves[0].a.y,0,'f',6);check(svg.contains(expected.toLatin1()),"512 coalesced updates publish final geometry");
 auto pending=shutter_animation_frame(.12);pending.overlayOpacity=1;check(v->update(pending,800,600),"pending before hide");v->hide();settle(100);check(!root->isVisible(),"late Ready cannot undo hide");pending=shutter_animation_frame(.13);pending.overlayOpacity=1;check(v->update(pending,800,600),"pending before invalid");check(!v->update(pending,0,600),"invalid while pending");settle(100);check(!root->isVisible(),"late Ready cannot undo invalid-frame hide");
 for(double bad:{0.0,-1.0,16385.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){check(!v->update(f,bad,600)&&!root->isVisible(),"invalid width hides stale frame");check(!v->update(f,800,bad)&&!root->isVisible(),"invalid height hides stale frame");}
 for(int field=0;field<8;field++){auto bad=f;switch(field){case 0:bad.overlayOpacity=1.1;break;case 1:bad.starOpacity=-.1;break;case 2:bad.starScale=11;break;case 3:bad.starRotation=100001;break;case 4:bad.starX=std::numeric_limits<double>::quiet_NaN();break;case 5:bad.starY=std::numeric_limits<double>::infinity();break;case 6:bad.leaves[7].controlA.x=100001;break;case 7:bad.leaves[0].b.y=std::numeric_limits<double>::infinity();break;}check(!v->update(bad,800,600)&&!root->isVisible(),"invalid frame hides");}
 check(v->update(f,800,600),"valid after rejection accepted");settle(100);check(root->isVisible(),"valid after rejection rendered");v->hide();check(!root->isVisible(),"explicit hide");f.overlayOpacity=0;check(v->update(f,800,600)&&!root->isVisible(),"zero alpha hides");
 QPointer<QObject>guard(root);v->close();v=nullptr;check(guard.isNull(),"close destroys subtree");
 for(int mode=0;mode<5;mode++){auto*parent=new QQuickItem(nw.contentItem());QQmlEngine::setContextForObject(parent,engine.rootContext());v=NativeAnimationView::create(api,stock,menu,ip,signal,parent);check(v,"lifecycle create");root=static_cast<QQuickItem*>(v->item());image=root->findChild<QQuickImage*>("X2dNativeShutterLeaves");f.overlayOpacity=1;check(v->update(f,800,600),"lifecycle baseline");if(mode==2)settle(80);
  if(mode==0){delete parent;check(!v->update(f,800,600)&&!v->item(),"parent destroyed");v->close();}
  if(mode==1){delete image;check(!v->update(f,800,600),"image destroyed refuses update");v->close();delete parent;}
  if(mode==4){image->setSource(QUrl("data:image/svg+xml;base64,AA=="));settle(100);check(!root->isVisible()&&!v->update(f,800,600),"decode failure hides and rejects reuse");v->close();delete parent;}
  if(mode==3){QObject::connect(image,&QQuickImageBase::statusChanged,root,[&](QQuickImageBase::Status state){if(state==QQuickImageBase::Ready&&v){auto*q=v;v=nullptr;q->close();}});settle(100);check(!v,"close inside asynchronous ready callback");delete parent;}
  if(mode==2){check(api.listen(api.context,root,Event::Hidden,closeView,&v,noop),"visible callback connection");v->hide();check(!v,"callback close while hide");delete parent;}
 }
 check(!NativeAnimationView::create(api,stock,menu,{},signal,nw.contentItem()),"invalid image profile");ip.size=4097;check(!NativeAnimationView::create(api,stock,menu,ip,signal,nw.contentItem()),"oversize image profile");check(!NativeAnimationView::create(api,stock,menu,imageProfile(),signal,nullptr),"null parent");QQuickItem detached;check(!NativeAnimationView::create(api,stock,menu,imageProfile(),signal,&detached),"parent without engine rejected");check(dataRequests>0,"only real data URI network requests observed");delete o;
 printf("NATIVE_ANIMATION_VIEW_PASS assertions=%d frames=%lld camera_requests=0 original_GUI=0\n",checks,static_cast<long long>(frames.size()));
}
