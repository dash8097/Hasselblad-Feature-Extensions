#ifndef HBL_BOOT_TRANSPORT_H
#define HBL_BOOT_TRANSPORT_H
#include <QtCore/qobject.h>
#include <QtCore/qbytearray.h>
#include <stdint.h>
void hbl_boot_sender(QObject *sender);
bool hbl_boot_reply(const QByteArray &bytes);
bool hbl_boot_start();
void hbl_boot_stop();
#endif
