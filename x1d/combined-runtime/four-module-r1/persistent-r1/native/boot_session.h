#pragma once
#include <QtCore/qbytearray.h>
// 仅由串行装载线程调用；所有 I/O 错误均锁定本次会话。
bool hbl_boot_exchange(const QByteArray &,QByteArray *);
bool hbl_boot_pause(unsigned);
void hbl_boot_load_run();

bool hbl_boot_is_batch_request(const QByteArray &);
