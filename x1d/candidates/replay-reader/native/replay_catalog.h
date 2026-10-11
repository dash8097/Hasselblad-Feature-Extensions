#ifndef X1D_REPLAY_CATALOG_H
#define X1D_REPLAY_CATALOG_H
#include <QtCore/qstringlist.h>
class QQmlEngine;
namespace X1D {
void updateCatalog(QQmlEngine *engine);
void invalidateCatalog();
void replaceCatalog(const QString &directory,const QStringList &raw,const QStringList &jpeg,bool complete);
bool knownJpegAbsent(const QString &source);
int catalogJpegPath(const QString &source,QString *path);
}
#endif
