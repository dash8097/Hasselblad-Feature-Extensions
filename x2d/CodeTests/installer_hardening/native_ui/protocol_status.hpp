#pragma once
#include "protocol_text.hpp"

#include <cstdint>
namespace x2dui {
// 固定本机文件协议；不开放 URL、路径或捕获请求写入口。
enum class PixelEndpoint { Status, Shutter, QuarterStatus, Arm, Quarter, Finish };
const char* pixel_endpoint_path(PixelEndpoint) noexcept;
struct ProtocolValue {
 enum class Type { Null, Boolean, Number, String, Container } type=Type::Null;
 bool boolean=false; double number=0; ProtocolText string;
};
struct ProtocolObject {
 struct Entry {ProtocolText key;ProtocolValue value;};
 Entry values[64];size_t count=0;
 const ProtocolValue* get(const char*) const noexcept;
 bool boolean(const char*) const noexcept;
 bool is_boolean(const char*) const noexcept;
 bool integer(const char*,uint64_t min,uint64_t max,uint64_t& result) const noexcept;
 uint64_t integer_or(const char*,uint64_t min,uint64_t max,uint64_t fallback=0) const noexcept;
 ProtocolText string(const char*) const;
};
// UTF-8/JSON、重复键、深度、长度均检查；未知字段允许但受同一资源上限。
bool parse_protocol(ProtocolBytes,ProtocolObject&) noexcept;
bool protocol_session(const ProtocolText&) noexcept;
UiText protocol_utf16(ProtocolBytes);
}
