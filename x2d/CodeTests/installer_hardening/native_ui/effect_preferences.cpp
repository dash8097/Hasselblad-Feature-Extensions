#include "effect_preferences.hpp"
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include <QtCore/QUrl>

LocalEffectTransport::LocalEffectTransport(QObject *parent):EffectTransport(parent){
    network.setRedirectPolicy(QNetworkRequest::ManualRedirectPolicy);
}
void LocalEffectTransport::request(int mode){
    if(mode < -1 || mode > 2){emit completed(0,{});return;}
    const auto suffix=mode<0?QStringLiteral("/mode"):QStringLiteral("/mode/")+QString::number(mode);
    QNetworkRequest request(QUrl(QStringLiteral("http://127.0.0.1:38491")+suffix));
    request.setTransferTimeout(3000);
    auto *reply=mode<0?network.get(request):network.post(request,QByteArray{});
    connect(reply,&QNetworkReply::finished,this,[this,reply]{
        const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QByteArray body=reply->read(2); // Only exactly one ASCII mode byte is valid.
        if(reply->bytesAvailable()>0)body.append('!');
        if(reply->error()!=QNetworkReply::NoError)body.clear();
        emit completed(status,body);reply->deleteLater();
    });
}
EffectPreferences::EffectPreferences(QQuickItem *parent,EffectTransport *provided):QQuickItem(parent),transport(provided){
    setVisible(false);if(!transport)transport=new LocalEffectTransport(this);
    connect(transport,&EffectTransport::completed,this,&EffectPreferences::receive);
    retry.setInterval(300);
    connect(&retry,&QTimer::timeout,this,[this]{if(!pending){++tries;emit attemptsChanged();requestMode(-1);updateTimer();}});
}
QStringList EffectPreferences::labels() const{return {QString::fromUtf8("关"),QString::fromUtf8("动画"),QString::fromUtf8("动画与声音")};}
void EffectPreferences::componentComplete(){QQuickItem::componentComplete();start();}
void EffectPreferences::start(){if(loaded)return;loaded=true;if(enabled)requestMode(-1);updateTimer();}
void EffectPreferences::setNetworkEnabled(bool value){if(enabled==value)return;enabled=value;emit networkEnabledChanged();updateTimer();}
void EffectPreferences::updateTimer(){if(loaded&&enabled&&!available&&tries<34)retry.start();else retry.stop();}
void EffectPreferences::selectMode(int next){requestMode(next);}
void EffectPreferences::requestMode(int next){
    if(!enabled||pending||(next!=-1&&(!available||next<0||next>2)))return;
    requested=next;pending=true;emit savingChanged();transport->request(next);
}
void EffectPreferences::receive(int status,const QByteArray&body){
    if(!pending)return;pending=false;emit savingChanged();
    bool valid=status==200&&body.size()==1&&body[0]>='0'&&body[0]<='2';
    if(valid){int n=body[0]-'0';if(current!=n){current=n;emit modeChanged();}if(!available){available=true;emit readyChanged();}if(!error.isEmpty()){error.clear();emit errorTextChanged();}}
    else{auto text=requested<0?QString::fromUtf8("效果设置暂不可用"):QString::fromUtf8("保存未确认，请重新读取");if(error!=text){error=text;emit errorTextChanged();}if(available){available=false;emit readyChanged();}if(requested>=0&&tries){tries=0;emit attemptsChanged();}}
    updateTimer();
}
extern "C" Q_DECL_EXPORT QQuickItem*x2d_create_effect_preferences(QQuickItem*parent,bool enabled){auto *item=new EffectPreferences(parent);item->setNetworkEnabled(enabled);item->start();return item;}
