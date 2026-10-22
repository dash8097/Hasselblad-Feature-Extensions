#pragma once
#include <cstddef>
#include <cstdint>
namespace cn { namespace unpack {
struct File {const char*name;const char*sha256;uint64_t size;unsigned mode;};
struct Bundle {const char*archive_name;const char*sha256;uint64_t compressed_size;const File*files;size_t count;};
} }
extern "C" int cn_unpack_main(int,char**);
