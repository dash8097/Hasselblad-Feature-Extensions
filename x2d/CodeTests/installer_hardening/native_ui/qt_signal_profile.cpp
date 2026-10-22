#include "qt_signal_profile.hpp"
#include <QLibrary>
#include <cstdlib>
template<class F>static F fn(const char*n){auto p=QLibrary::resolve("Qt6Core",n);if(!p)std::abort();return reinterpret_cast<F>(p);}
x2dui::SignalProfile qt_signal_profile(){x2dui::SignalProfile p{};
 p.index=fn<decltype(p.index)>("_ZNK11QMetaObject13indexOfSignalEPKc");p.method=fn<decltype(p.method)>("_ZNK11QMetaObject6methodEi");p.signal_index=fn<decltype(p.signal_index)>("_ZN18QMetaObjectPrivate11signalIndexERK11QMetaMethod");p.connect=fn<decltype(p.connect)>("_ZN14QObjectPrivate7connectEPK7QObjectiS2_PN9QtPrivate15QSlotObjectBaseEN2Qt14ConnectionTypeE");p.disconnect=fn<decltype(p.disconnect)>("_ZN7QObject10disconnectERKN11QMetaObject10ConnectionE");p.connection_dtor=fn<decltype(p.connection_dtor)>("_ZN11QMetaObject10ConnectionD1Ev");
return p;}
