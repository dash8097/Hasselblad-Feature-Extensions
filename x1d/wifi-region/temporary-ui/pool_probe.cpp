#include <QtGui/QGuiApplication>
#include <QtQml/QQmlApplicationEngine>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
// No camera modules, no native device context, no display window.
int main(int argc,char **argv) {
    QGuiApplication app(argc,argv);
    QQmlApplicationEngine engine;
    engine.load(QUrl::fromLocalFile(QString::fromLocal8Bit(argv[1])));
    if(engine.rootObjects().isEmpty())return 2;
    QTimer::singleShot(8000,&app,SLOT(quit()));
    app.exec();
    return engine.rootObjects().first()->property("passed").toBool()?0:3;
}
