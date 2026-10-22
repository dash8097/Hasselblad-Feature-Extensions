#include "qt_binding_profile.hpp"
#include <QQmlProperty>
#include <QtQml/private/qqmlanybinding_p.h>
#include <cstring>
static_assert(sizeof(QQmlAnyBinding)==8&&sizeof(QQmlProperty)==8,"fixed binding ABI");
static x2dui::AbiBinding transfer(QQmlAnyBinding b){x2dui::AbiBinding r;std::memcpy(&r.pointer,&b,8);std::memset(static_cast<void*>(&b),0,8);return r;}
x2dui::BindingProfile qt_binding_profile(){return {
 [](const void*p){return transfer(QQmlAnyBinding::takeFrom(*static_cast<const QQmlProperty*>(p)));},
 [](const void*p){return transfer(QQmlAnyBinding::ofProperty(*static_cast<const QQmlProperty*>(p)));},
 [](void*b,const void*p,bool mode){static_cast<QQmlAnyBinding*>(b)->installOn(*static_cast<const QQmlProperty*>(p),mode?QQmlAnyBinding::RespectInterceptors:QQmlAnyBinding::IgnoreInterceptors);},
 [](void*b,const x2dui::AbiBinding&v)->void*{*static_cast<QQmlAnyBinding*>(b)=*reinterpret_cast<const QQmlAnyBinding*>(&v);return b;},
 [](const void*p)->x2dui::AbiVariant{QVariant v=static_cast<const QQmlProperty*>(p)->read();x2dui::AbiVariant r;new(&r)QVariant(v);return r;},
 [](const void*p){return static_cast<const QQmlProperty*>(p)->isValid();},[](const void*p){return static_cast<const QQmlProperty*>(p)->isWritable();},
 [](const x2dui::AbiVariant*a,const x2dui::AbiVariant*b){return *reinterpret_cast<const QVariant*>(a)==*reinterpret_cast<const QVariant*>(b);}
 };}
