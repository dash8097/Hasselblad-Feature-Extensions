#ifndef X1D_REPLAY_RETRY_H
#define X1D_REPLAY_RETRY_H
#include <QtCore/qstring.h>
namespace X1D {
enum RetryCause { ResourceBusy, ContextUnavailable, UploadRejected };
void selectFullSource(const QString &source);
bool currentFullSource(const QString &source);
void retryFull(const QString &source, RetryCause cause);
bool consumeFullRetry(const QString &source);
}
#endif
