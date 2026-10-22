#pragma once
#include "drive_settings_session.hpp"
#include "factory_proxy.hpp"
namespace x2dui {
// Main-thread only. Invocation is never acknowledgement. Each setter is sent
// once, followed by a later-tick typed readback; uncertainty never retries it.
class DriveProxyAdapter {
 FactoryProxyProfile factory;PixelController&controller;int values[6]{};
 DriveSessionIntent pending{};uint64_t deadline=0,started=0,statusBefore=0,shutterBefore=0;ProtocolText session;
 bool resultReady=false;DriveSessionResult result=DriveSessionResult::Unknown;
 int64_t expected=0;
 void done(DriveSessionResult r){result=r;resultReady=true;}
public:
 DriveProxyAdapter(FactoryProxyProfile p,PixelController&c,const int*v):factory(p),controller(c){for(unsigned i=0;i<6;i++)values[i]=v[i];}
 bool begin(const DriveSessionIntent&,uint64_t now);
 void tick(uint64_t now);
 bool take_result(uint64_t&id,DriveSessionResult&);
 void cancel(){pending={};resultReady=false;session.clear();}
 bool active()const{return pending.id!=0;}
};
}
