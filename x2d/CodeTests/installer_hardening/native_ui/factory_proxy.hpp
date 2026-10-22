#pragma once
#include "abi_view_api.hpp"
#include <cstdint>
namespace x2dui {
enum class FactoryField:unsigned {ImageFormat,DriveMode,ExitDriveMode,ForwardInput,EvfOnly,ScreenTimeout,ScreensActive,ScreensAllowed,StorageBrowsing,CaptureStorage,ExposureMode,ImageBitDepth,ShutterCurrent,ShutterMin,ShutterMax,ElectronicShutter,ExposureStatus,Count};
struct FactoryFieldSpec {unsigned proxy,index,width;bool boolean,signedValue,writable;};
struct FactoryProxyProfile {void*const*slots[3]{};const void*meta[3]{};AbiMetaCall call[3]{};const void*(*cast)(const void*,const void*);FactoryFieldSpec fields[static_cast<unsigned>(FactoryField::Count)]{};};
// No factory/getInstance invocation. A missing original instance is unavailable,
// never a request to connect or initialize a new camera proxy.
bool factory_read(const FactoryProxyProfile&,FactoryField,int64_t&);
bool factory_write(const FactoryProxyProfile&,FactoryField,int64_t);
}
