#pragma once
#include <QtQuick/QQuickItem>
#include <QtCore/QTimer>
#include <QtCore/QStringList>
#include <QtNetwork/QNetworkAccessManager>

// Only a fixed local service contract is exposed; no script/eval/URL execution surface.
class EffectTransport : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual void request(int mode)=0;
signals:
    void completed(int status,QByteArray body);
};
class LocalEffectTransport final : public EffectTransport {
    Q_OBJECT
    QNetworkAccessManager network;
public:
    explicit LocalEffectTransport(QObject *parent=nullptr);
    void request(int mode) override;
};
class EffectPreferences final : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(bool networkEnabled READ networkEnabled WRITE setNetworkEnabled NOTIFY networkEnabledChanged)
    Q_PROPERTY(int mode READ mode NOTIFY modeChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
    Q_PROPERTY(bool saving READ saving NOTIFY savingChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged)
    Q_PROPERTY(int attempts READ attempts NOTIFY attemptsChanged)
    Q_PROPERTY(QStringList labels READ labels CONSTANT)
    EffectTransport *transport;
    QTimer retry;
    bool enabled=false,loaded=false,available=false,pending=false;
    int current=0,tries=0,requested=-1;
    QString error;
    void updateTimer();
    void requestMode(int next);
    void receive(int status,const QByteArray &body);
protected:
    void componentComplete() override;
public:
    explicit EffectPreferences(QQuickItem *parent=nullptr,EffectTransport *transport=nullptr);
    bool networkEnabled() const{return enabled;}
    int mode() const{return current;}
    bool ready() const{return available;}
    bool saving() const{return pending;}
    int attempts() const{return tries;}
    QString errorText() const{return error;}
    QStringList labels() const;
    void setNetworkEnabled(bool value);
    void start();
    Q_INVOKABLE void selectMode(int next);
signals:
    void networkEnabledChanged();
    void modeChanged();
    void readyChanged();
    void savingChanged();
    void errorTextChanged();
    void attemptsChanged();
};
extern "C" Q_DECL_EXPORT QQuickItem *x2d_create_effect_preferences(QQuickItem *parent,bool networkEnabled);
