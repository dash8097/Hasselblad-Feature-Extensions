#ifndef HBL_COEXIST_WPA_MONITOR_H
#define HBL_COEXIST_WPA_MONITOR_H
#include "link_evidence.h"
#include "rf_local_socket.h"
#include <QtCore/qobject.h>
#include <QtCore/qsocketnotifier.h>
#include <QtCore/qtimer.h>
#include <QtCore/qbytearray.h>
#include <arpa/inet.h>
#include <functional>

namespace coexist {
// 本候选启动的独立 supplicant 控制口；不使用全局广播控制口。
class WpaMonitor : public QObject {
public:
    explicit WpaMonitor(QObject *parent=nullptr):QObject(parent) {
        events=openSocket("/tmp/hbl-flash-wifi-coexist/wpa-events.sock");
        queries=openSocket("/tmp/hbl-flash-wifi-coexist/wpa-query.sock");
        if(events<0 || queries<0) {evidence.gap=true;return;}
        eventNotifier=new QSocketNotifier(events,QSocketNotifier::Read,this);
        queryNotifier=new QSocketNotifier(queries,QSocketNotifier::Read,this);
        QObject::connect(eventNotifier,&QSocketNotifier::activated,this,[this](int){readEvents();});
        QObject::connect(queryNotifier,&QSocketNotifier::activated,this,[this](int){readStatus();});
        if(::send(events,"ATTACH",6,MSG_NOSIGNAL)!=6) {evidence.gap=true;return;}
        attachAt=rf_monotonic_ms();
        timer.setInterval(250);
        QObject::connect(&timer,&QTimer::timeout,this,[this](){poll();});timer.start();
    }
    ~WpaMonitor() override {
        timer.stop();
        if(events>=0) {::send(events,"DETACH",6,MSG_DONTWAIT|MSG_NOSIGNAL);::close(events);}
        if(queries>=0) ::close(queries);
        ::unlink("/tmp/hbl-flash-wifi-coexist/wpa-events.sock");
        ::unlink("/tmp/hbl-flash-wifi-coexist/wpa-query.sock");
    }
    Evidence evidence;
    bool completed=false;
    unsigned frequency=0;
    uint64_t lastStatus=0;
    std::function<void()> changed;
    bool ready() const {
        const uint64_t now=rf_monotonic_ms();
        return evidence.attached && completed && frequency>=5000 && frequency<6000 && address &&
               lastStatus && now>=lastStatus && now-lastStatus<1500;
    }
    bool beginWindow() {
        if(!ready()) return false;
        // 同一控制连接持续监听；不重启监听来隐去最初的断线事件。
        evidence=Evidence();evidence.attached=true;evidence.baselineReady=true;
        initialAddress=address;initialPeer=peerFingerprint;
        evidence.awayAt=rf_monotonic_ms();return true;
    }
    bool finishWindow() {
        if(!ready()) return false;
        evidence.recovered=true;evidence.returnAt=rf_monotonic_ms();
        return true;
    }
    bool addressUnchanged() const {return address && address==initialAddress;}
    bool peerUnchanged() const {return peerFingerprint && peerFingerprint==initialPeer;}
private:
    int events=-1,queries=-1;
    QSocketNotifier *eventNotifier=nullptr,*queryNotifier=nullptr;
    QTimer timer;
    uint64_t attachAt=0,queryAt=0,peerFingerprint=0,initialPeer=0;
    uint32_t address=0,initialAddress=0;
    static int openSocket(const char *localPath) {
        const int fd=rf_local_bind(localPath);
        if(fd<0) return -1;
        sockaddr_un target={};
        const int length=rf_local_address(&target,"/tmp/hbl-flash-wifi-coexist/wpa/wlp1s0");
        if(!length || ::connect(fd,reinterpret_cast<sockaddr *>(&target),socklen_t(length))) {::close(fd);return -1;}
        return fd;
    }
    void publish() {if(changed) changed();}
    void poll() {
        const uint64_t now=rf_monotonic_ms();
        if(!evidence.attached && now-attachAt>=1500) {evidence.gap=true;timer.stop();publish();return;}
        if(queryAt && now-queryAt>=1500) {
            // 响应无法和新请求区分时永久停用查询，避免把迟到回复当新快照。
            evidence.gap=true;completed=false;timer.stop();publish();return;
        }
        if(evidence.attached && !queryAt) {
            if(::send(queries,"STATUS",6,MSG_DONTWAIT|MSG_NOSIGNAL)!=6) {evidence.gap=true;completed=false;publish();return;}
            queryAt=now;
        }
        publish();
    }
    void readEvents() {
        for(unsigned n=0;n<128;++n) {
            char bytes[4096];const ssize_t size=::recv(events,bytes,sizeof(bytes),MSG_DONTWAIT|MSG_TRUNC);
            if(size<0 && (errno==EAGAIN || errno==EWOULDBLOCK)) return;
            if(size<=0 || size>ssize_t(sizeof(bytes))) {evidence.event(LinkEvent::Overflow);publish();return;}
            if(size==3 && !memcmp(bytes,"OK\n",3)) {evidence.attached=true;publish();continue;}
            if(size==5 && !memcmp(bytes,"FAIL\n",5)) {evidence.gap=true;evidence.attached=false;publish();continue;}
            const LinkEvent event=parseEvent(bytes,size_t(size));
            evidence.event(event);
            if(event==LinkEvent::Disconnected || event==LinkEvent::Terminating) completed=false;
            publish();
        }
        // 有界消费；下个事件循环继续收取，不将预算耗尽误记为丢包。
    }
    void readStatus() {
        char bytes[4096];const ssize_t size=::recv(queries,bytes,sizeof(bytes),MSG_DONTWAIT|MSG_TRUNC);
        if(size<0 && (errno==EAGAIN || errno==EWOULDBLOCK)) return;
        if(!queryAt || size<=0 || size>ssize_t(sizeof(bytes)) || memchr(bytes,0,size_t(size))) {
            evidence.gap=true;completed=false;timer.stop();publish();return;
        }
        queryAt=0;lastStatus=rf_monotonic_ms();completed=false;frequency=0;address=0;peerFingerprint=0;
        const QList<QByteArray> lines=QByteArray(bytes,int(size)).split('\n');
        unsigned stateFields=0,freqFields=0,addressFields=0,peerFields=0;
        for(const QByteArray &line:lines) {
            if(line.startsWith("wpa_state=")) {++stateFields;completed=line=="wpa_state=COMPLETED";}
            else if(line.startsWith("freq=")) {++freqFields;bool ok=false;frequency=line.mid(5).toUInt(&ok);if(!ok)frequency=0;}
            else if(line.startsWith("ip_address=")) {++addressFields;in_addr addr={};if(inet_pton(AF_INET,line.mid(11).constData(),&addr)==1)address=addr.s_addr;}
            else if(line.startsWith("bssid=")) {
                ++peerFields;const QByteArray peer=line.mid(6);if(peer.size()!=17) continue;
                uint64_t hash=UINT64_C(1469598103934665603);bool valid=true;
                for(int i=0;i<peer.size();++i) {
                    const char c=peer[i];if(i%3==2 ? c!=':' : !((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))valid=false;
                    hash=(hash^uint8_t(c))*UINT64_C(1099511628211);
                }
                if(valid)peerFingerprint=hash;
            }
        }
        if(stateFields!=1 || freqFields>1 || addressFields>1 || peerFields>1 || (completed && (!frequency || !peerFingerprint))) {
            evidence.gap=true;completed=false;
        }
        publish();
    }
};
}
#endif
