// 只遍历 GUI 已有对象；不创建业务页，不调用相机业务方法。
#include <QtCore/qelapsedtimer.h>
#include <QtCore/qset.h>
#include <QtCore/qtimer.h>
#include <QtCore/qvariant.h>
#include <QtGui/qguiapplication.h>
#include <QtQuick/qquickitem.h>
#include <QtQuick/qquickwindow.h>
#include "gate_policy.h"
#include "catalog.h"

namespace {
QList<QObject *> residentObjects(QObject *root) {
    QList<QObject *> out, pending; QSet<QObject *> seen;
    if (root) pending.append(root);
    while (!pending.isEmpty()) {
        QObject *o = pending.takeLast();
        if (seen.contains(o)) continue;
        seen.insert(o); out.append(o); pending.append(o->children());
        if (QQuickItem *i = qobject_cast<QQuickItem *>(o))
            for (QQuickItem *child : i->childItems()) pending.append(child);
        if (QQuickWindow *w = qobject_cast<QQuickWindow *>(o)) pending.append(w->contentItem());
    }
    return out;
}
bool named(QObject *o, const char *name) { return o->objectName() == QString::fromLatin1(name); }
struct Snapshot { bool complete = true; bool error = false; };
void checkPage(QObject *page, bool menu, Snapshot &s) {
    if (!page->property("residentPrepared").toBool()) s.complete = false;
    int lists = 0;
    for (QObject *o : residentObjects(page)) {
        // 嵌套 Generic 的行属于各自页面，不能计入 Menu。
        if (!named(o, menu ? "Menu_list" : "SettingsGeneric_list")) continue;
        ++lists; int rows = 0, loaders = 0;
        for (QObject *r : residentObjects(o)) {
            if (named(r, menu ? "listDelegate" : "baseItem")) ++rows;
            if (!menu && named(r, "specialItem")) {
                ++loaders; int state = r->property("status").toInt();
                if (state == 3) s.error = true;
                if (state != 1) s.complete = false;
            }
        }
        int count = o->property("count").toInt();
        if (count < 1 || rows != count || (!menu && loaders != count)) s.complete = false;
    }
    if (lists != 1) { s.complete = false; if (lists > 1) s.error = true; }
}
Snapshot residentSnapshot(QQmlApplicationEngine *engine) {
    Snapshot s; QSet<QObject *> objects;
    for (QObject *rootObject : engine->rootObjects())
        for (QObject *o : residentObjects(rootObject)) objects.insert(o);
    for (QWindow *w : QGuiApplication::allWindows())
        for (QObject *o : residentObjects(w)) objects.insert(o);
    int screens = 0, topPools = 0, childPools = 0;
    QSet<QString> menus, pages;
    for (QObject *o : objects) {
        if (named(o, "MainScreen_root")) ++screens;
        if (named(o, "menu_Loader")) ++topPools;
        if (named(o, "entry_loader")) ++childPools;
        if (!named(o, "residentNativeLoader")) continue;
        int state = o->property("status").toInt();
        if (state == 3) s.error = true;
        if (state != 1) { s.complete = false; continue; }
        QQuickItem *loader = qobject_cast<QQuickItem *>(o);
        if (!loader) { s.error = true; continue; }
        QList<QQuickItem *> roots = loader->childItems();
        if (roots.size() != 1) { s.complete = false; continue; }
        QObject *page = roots.first();
        QString key = o->property("slotKey").toString();
        bool menu = named(page, "Menu_root");
        if (!menu && !named(page, "SettingsGeneric_root")) { s.error = true; continue; }
        QSet<QString> &keys = menu ? menus : pages;
        if (keys.contains(key)) s.error = true;
        keys.insert(key);
        if (!menu && page->property("itemValues").toString() != key) s.error = true;
        checkPage(page, menu, s);
    }
    if (screens > 1 || topPools > 1 || childPools > 3) s.error = true;
    if (screens != 1 || topPools != 1 || childPools != 3) s.complete = false;
    if (menus != expectedMenus() || pages != expectedPages()) s.complete = false;
    if (!(menus - expectedMenus()).isEmpty() || !(pages - expectedPages()).isEmpty()) s.error = true;
    return s;
}
struct BootGate : QObject {
    QQmlApplicationEngine *engine;
    QTimer timer;
    QElapsedTimer elapsed;
    ResidentGate::Policy policy;
    bool warning = false;
    explicit BootGate(QQmlApplicationEngine *e) : QObject(e), engine(e) {
        elapsed.start();
        QObject::connect(e, &QQmlEngine::warnings, this, [this](const QList<QQmlError> &errors) {
            // URL 仅用于判定，不记录属性值或完整错误文本。
            for (const QQmlError &error : errors) {
                QString path = error.url().path();
                if (path.startsWith(QStringLiteral("/mainmenu/")) || path.startsWith(QStringLiteral("/settings/"))) warning = true;
            }
        });
        QObject::connect(&timer, &QTimer::timeout, this, [this]() {
            Snapshot s = residentSnapshot(engine);
            ResidentGate::Result r = policy.sample(s.complete, s.error || warning, elapsed.elapsed());
            if (r == ResidentGate::Pending) return;
            timer.stop();
            status(r == ResidentGate::Ready ? "ui-resident-ready-resources6-components5-pools3-pages23-rows" :
                   r == ResidentGate::TimedOut ? "ui-resident-pool-timeout" : "ui-resident-pool-failed");
        });
    }
    void start() { status("ui-resident-pool-pending"); timer.start(200); }
};
}
