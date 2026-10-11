// 只读既有 GUI 对象。诊断只包含固定类别名、计数、Loader 状态和经过时间。
#include <QtCore/qelapsedtimer.h>
#include <QtCore/qsavefile.h>
#include <QtCore/qset.h>
#include <QtCore/qtimer.h>
#include <QtCore/qvariant.h>
#include <QtGui/qguiapplication.h>
#include <QtQuick/qquickitem.h>
#include <QtQuick/qquickwindow.h>
#include "gate_policy.h"
#include "catalog.h"

namespace {
QList<QObject *> residentObjects(QObject *rootObject) {
    QList<QObject *> out, pending; QSet<QObject *> seen;
    if (rootObject) pending.append(rootObject);
    while (!pending.isEmpty()) {
        QObject *o = pending.takeLast();
        if (seen.contains(o)) continue;
        seen.insert(o); out.append(o); pending.append(o->children());
        if (QQuickItem *item = qobject_cast<QQuickItem *>(o))
            for (QQuickItem *child : item->childItems()) pending.append(child);
        if (QQuickWindow *window = qobject_cast<QQuickWindow *>(o)) pending.append(window->contentItem());
    }
    return out;
}
bool named(QObject *o, const char *name) { return o->objectName() == QString::fromLatin1(name); }
QString fixedKey(const QString &key) {
    return expectedMenus().contains(key) || expectedPages().contains(key) ? key : QStringLiteral("other");
}
struct PoolDiagnostic {
    QString key, kind;
    int loader = -1, item = 0, prepared = 0, lists = 0, model = 0;
    int delegates = 0, rowLoaders = 0, rowReady = 0, rowErrors = 0;
};
struct Snapshot {
    bool complete = true, error = false;
    int screens = 0, topPools = 0, childPools = 0;
    int nativePools = 0, nativeNull = 0, nativeLoading = 0, nativeReady = 0, nativeErrors = 0;
    int menus = 0, pages = 0, prepared = 0, lists = 0;
    int modelRows = 0, delegates = 0, rowLoaders = 0, rowReady = 0, rowErrors = 0;
    QList<PoolDiagnostic> pools;
    QByteArray text(long long elapsed, int warnings) const {
        QByteArray out = "schema=1 t=" + QByteArray::number(elapsed) +
            " screen=" + QByteArray::number(screens) + " top=" + QByteArray::number(topPools) +
            " child=" + QByteArray::number(childPools) + " native=" + QByteArray::number(nativePools) +
            " null=" + QByteArray::number(nativeNull) + " loading=" + QByteArray::number(nativeLoading) +
            " ready=" + QByteArray::number(nativeReady) + " error=" + QByteArray::number(nativeErrors) +
            " menus=" + QByteArray::number(menus) + " pages=" + QByteArray::number(pages) +
            " prepared=" + QByteArray::number(prepared) + " lists=" + QByteArray::number(lists) +
            " model=" + QByteArray::number(modelRows) + " delegates=" + QByteArray::number(delegates) +
            " rowloaders=" + QByteArray::number(rowLoaders) + " rowready=" + QByteArray::number(rowReady) +
            " rowerrors=" + QByteArray::number(rowErrors) + " warnings=" + QByteArray::number(warnings) + "\n";
        for (const PoolDiagnostic &p : pools)
            out += "pool key=" + p.key.toLatin1() + " kind=" + p.kind.toLatin1() +
                " loader=" + QByteArray::number(p.loader) + " item=" + QByteArray::number(p.item) +
                " prepared=" + QByteArray::number(p.prepared) + " lists=" + QByteArray::number(p.lists) +
                " model=" + QByteArray::number(p.model) + " delegates=" + QByteArray::number(p.delegates) +
                " rowloaders=" + QByteArray::number(p.rowLoaders) + " rowready=" + QByteArray::number(p.rowReady) +
                " rowerrors=" + QByteArray::number(p.rowErrors) + "\n";
        return out;
    }
};
void checkPage(QObject *page, bool menu, PoolDiagnostic &p, Snapshot &s) {
    p.prepared = page->property("residentPrepared").toBool() ? 1 : 0;
    s.prepared += p.prepared;
    if (!p.prepared) s.complete = false;
    for (QObject *o : residentObjects(page)) {
        if (!named(o, menu ? "Menu_list" : "SettingsGeneric_list")) continue;
        ++p.lists; ++s.lists;
        int count = o->property("count").toInt(); p.model += count; s.modelRows += count;
        for (QObject *row : residentObjects(o)) {
            if (named(row, menu ? "listDelegate" : "baseItem")) { ++p.delegates; ++s.delegates; }
            if (!menu && named(row, "specialItem")) {
                ++p.rowLoaders; ++s.rowLoaders;
                int state = row->property("status").toInt();
                if (state == 1) { ++p.rowReady; ++s.rowReady; }
                if (state == 3) { ++p.rowErrors; ++s.rowErrors; s.error = true; }
                if (state != 1) s.complete = false;
            }
        }
        if (count < 1 || p.delegates != count || (!menu && p.rowLoaders != count)) s.complete = false;
    }
    if (p.lists != 1) { s.complete = false; if (p.lists > 1) s.error = true; }
}
Snapshot residentSnapshot(QQmlApplicationEngine *engine) {
    Snapshot s; QSet<QObject *> objects;
    for (QObject *rootObject : engine->rootObjects())
        for (QObject *o : residentObjects(rootObject)) objects.insert(o);
    for (QWindow *window : QGuiApplication::allWindows())
        for (QObject *o : residentObjects(window)) objects.insert(o);
    QSet<QString> menus, pages;
    for (QObject *o : objects) {
        if (named(o, "MainScreen_root")) ++s.screens;
        if (named(o, "menu_Loader")) ++s.topPools;
        if (named(o, "entry_loader")) ++s.childPools;
        if (!named(o, "residentNativeLoader")) continue;
        ++s.nativePools; PoolDiagnostic p; p.loader = o->property("status").toInt();
        QString rawKey = o->property("slotKey").toString(); p.key = fixedKey(rawKey);
        p.kind = expectedMenus().contains(rawKey) ? QStringLiteral("menu") :
                 expectedPages().contains(rawKey) ? QStringLiteral("page") : QStringLiteral("other");
        if (p.loader == 0) ++s.nativeNull;
        else if (p.loader == 1) ++s.nativeReady;
        else if (p.loader == 2) ++s.nativeLoading;
        else if (p.loader == 3) { ++s.nativeErrors; s.error = true; }
        if (p.loader != 1) { s.complete = false; s.pools.append(p); continue; }
        // QQuickLoader 的公开 item 属性是权威装载结果；不再假设 childItems 恰有一个。
        QObject *page = o->property("item").value<QObject *>();
        p.item = page ? 1 : 0;
        if (!page) { s.complete = false; s.pools.append(p); continue; }
        bool menu = named(page, "Menu_root");
        if (!menu && !named(page, "SettingsGeneric_root")) { s.error = true; s.pools.append(p); continue; }
        if ((menu && p.kind != QStringLiteral("menu")) || (!menu && p.kind != QStringLiteral("page"))) s.error = true;
        QSet<QString> &keys = menu ? menus : pages;
        if (keys.contains(rawKey)) s.error = true;
        keys.insert(rawKey);
        if (!menu && page->property("itemValues").toString() != rawKey) s.error = true;
        checkPage(page, menu, p, s); s.pools.append(p);
    }
    s.menus = menus.size(); s.pages = pages.size();
    if (s.screens > 1 || s.topPools > 1 || s.childPools > 3) s.error = true;
    if (s.screens != 1 || s.topPools != 1 || s.childPools != 3) s.complete = false;
    if (menus != expectedMenus() || pages != expectedPages()) s.complete = false;
    if (!(menus - expectedMenus()).isEmpty() || !(pages - expectedPages()).isEmpty()) s.error = true;
    return s;
}
void saveDiagnostic(const QByteArray &value) {
    QSaveFile file(QString::fromLatin1(root) + QStringLiteral("/ui.diag"));
    if (file.open(QIODevice::WriteOnly)) { file.write(value); file.commit(); }
}
struct BootGate : QObject {
    QQmlApplicationEngine *engine;
    QTimer timer;
    QElapsedTimer elapsed;
    ResidentGate::Policy policy;
    int warningCount = 0;
    QByteArray lastDiagnostic;
    long long nextDiagnostic = 0;
    explicit BootGate(QQmlApplicationEngine *value) : QObject(value), engine(value) {
        elapsed.start();
        QObject::connect(value, &QQmlEngine::warnings, this, [this](const QList<QQmlError> &errors) {
            for (const QQmlError &error : errors) {
                QString path = error.url().path();
                if (path.startsWith(QStringLiteral("/mainmenu/")) || path.startsWith(QStringLiteral("/settings/"))) ++warningCount;
            }
        });
        QObject::connect(&timer, &QTimer::timeout, this, [this]() {
            Snapshot snapshot = residentSnapshot(engine); long long now = elapsed.elapsed();
            QByteArray structure = snapshot.text(0, warningCount);
            if (structure != lastDiagnostic || now >= nextDiagnostic) {
                saveDiagnostic(snapshot.text(now, warningCount)); lastDiagnostic = structure; nextDiagnostic = now + 2000;
            }
            ResidentGate::Result result = policy.sample(snapshot.complete, snapshot.error || warningCount, now);
            if (result == ResidentGate::Pending) return;
            timer.stop(); saveDiagnostic(snapshot.text(now, warningCount));
            status(result == ResidentGate::Ready ? "ui-resident-ready-resources6-components5-pools3-pages23-rows-diag1" :
                   result == ResidentGate::TimedOut ? "ui-resident-pool-timeout" : "ui-resident-pool-failed");
        });
    }
    void start() { status("ui-resident-pool-pending"); saveDiagnostic("schema=1 t=0 state=starting\n"); timer.start(200); }
};
}
