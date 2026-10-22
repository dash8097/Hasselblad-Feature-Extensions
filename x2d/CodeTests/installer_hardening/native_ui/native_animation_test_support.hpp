#pragma once
// 仅内部离屏测试；原QML与原生视图在相同字体/光栅环境逐帧比较。
#include <QCoreApplication>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QQuickItem>
#include <QFontDatabase>
#include <QRawFont>
#include <QImageReader>
#include <QQmlNetworkAccessManagerFactory>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <atomic>
#include <QDir>
#include <QElapsedTimer>
#include <QThread>
#include <QJsonObject>
#include <QImage>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
namespace animation_test {
inline int checks=0;inline std::atomic<unsigned>dataRequests{0};
class DataOnlyNetwork final:public QNetworkAccessManager {
public:explicit DataOnlyNetwork(QObject*p):QNetworkAccessManager(p){}
protected:QNetworkReply*createRequest(Operation op,const QNetworkRequest&r,QIODevice*data=nullptr)override{if(r.url().scheme()!=QStringLiteral("data")){std::fprintf(stderr,"NON_DATA_NETWORK_REQUEST_BLOCKED\n");std::abort();}++dataRequests;return QNetworkAccessManager::createRequest(op,r,data);}
};
struct DataOnlyFactory final:QQmlNetworkAccessManagerFactory {QNetworkAccessManager*create(QObject*p)override{return new DataOnlyNetwork(p);}};
inline void check(bool ok,const char*why){++checks;if(!ok){std::fprintf(stderr,"NATIVE_ANIMATION_VIEW_FAIL check=%d reason=%s\n",checks,why);std::exit(1);}}
inline void settle(int ms){QElapsedTimer t;t.start();do{QCoreApplication::processEvents();QThread::msleep(1);}while(t.elapsed()<ms);}
inline void fonts(QGuiApplication&app,const QDir&oracle){
 for(const char*n:{"segoeui.ttf","seguisym.ttf","msyh.ttc"})check(QFontDatabase::addApplicationFont(oracle.filePath(QString("private-fonts/")+n))>=0,"explicit private font");
 QFont f("Microsoft YaHei");f.setPixelSize(40);app.setFont(f);QRawFont raw=QRawFont::fromFont(f);check(raw.isValid()&&raw.supportsCharacter(0x2606)&&raw.supportsCharacter('C'),"actual star and Latin glyphs");
 check(QImageReader::supportedImageFormats().contains("svg"),"QtSvg image plugin available");
}
inline QImage capture(QQuickWindow&w,QSize size){w.update();settle(80);auto image=w.grabWindow().convertToFormat(QImage::Format_ARGB32_Premultiplied);check(image.size()==size,"frame capture dimensions");return image;}
inline unsigned descendants(const QObject*o){unsigned n=0;for(auto*c:o->children())n+=1+descendants(c);return n;}
inline QJsonObject comparison(const QImage&a,const QImage&b,const QString&tag,const QDir&out){
 check(a.size()==b.size(),"same image dimensions");uint64_t sum=0,changed=0;int maximum=0;
 for(int y=0;y<a.height();++y)for(int x=0;x<a.width();++x){auto p=a.pixel(x,y),q=b.pixel(x,y);int d=std::max({std::abs(qRed(p)-qRed(q)),std::abs(qGreen(p)-qGreen(q)),std::abs(qBlue(p)-qBlue(q)),std::abs(qAlpha(p)-qAlpha(q))});sum+=unsigned(d);if(d>16)++changed;maximum=std::max(maximum,d);}
 double count=double(a.width())*a.height(),mean=sum/count,fraction=changed/count;
 check(a.save(out.filePath(tag+"-qml.png"))&&b.save(out.filePath(tag+"-native.png")),"same captured frame saved as evidence");
 return {{"frame",tag},{"mean_channel_max_error",mean},{"pixels_error_gt16",double(changed)},{"fraction_error_gt16",fraction},{"max_error",maximum}};
}
}
