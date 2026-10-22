// X2D 4.2.0 固定原生叶节点；不含 runtime/setup 自身摘要。
#pragma once
namespace cn { namespace runtime {
struct Guard {const char* path;const char* digest;};
static constexpr Guard guards[]={
    {"/system/bin/x2d-pixel-overclock/libx2d_private_memory_capture.so","9df3e1da3832cc2a6626c281f605d68314202f19a622e3126fb53b408909be94"},
    {"/system/bin/x2d-pixel-overclock/render-memory","fdb428860cd7c04c673b523201520230da5efc9efdbd613100c37a328e42b32b"},
    {"/system/bin/x2d-pixel-overclock/fix-3fr","2d0e587626dcd5f5a39d978b0a3a1dfa3b63f898062fbc1a33a3b1847812dd4d"},
    {"/system/bin/x2d-pixel-overclock/pack","9521d4cf0a2ea01b039d71d991d507042c518c86c477ca7d5295912c19314b28"},
    {"/system/bin/x2d-pixel-overclock/preview","3b2c3e619c86785484368a60246f47158dd022380e528f9966a4e434a08e3a68"},
    {"/system/bin/x2d-pixel-overclock/inputs","aed57a4a9f35e3d3fd90f895613b0c7ab207c6b2f767f5bce6d0be8e0f8f9564"},
    {"/system/bin/x2d-pixel-overclock/heif","41f17819e58831d8d60a362ce17f54b49e4dc0f0e061ec940bff9608ab4f2d4a"},
    {"/system/bin/x2d-pixel-overclock/render-probe","1355b962f46ba01ee511dd436aa25d8cfda73c21d41d75a6bd0c496bd176f846"},
    {"/system/bin/x2d-multi-resolution/libquarter_capture.so","032ad9496b99e99991838db31c4d2ff57e85621dad6b9d8ba81074b94032eb87"},
    {"/system/bin/x2d-multi-resolution/finalize25","81e74ff2a0a5c363111f1c7f53084f38f08999c4b847cc8de53c006d3e98a8b4"},
    {"/system/bin/x2d-multi-resolution/render25","ccab2c22a8b7263a06448a3a9fe08f0817202d43004b78784caad7e9b73a0b5a"},
    {"/system/bin/x2d-multi-resolution/pack25","7f49110b1bfdd06b8bcd0d481954c89d83f7c03177f8af7e58d8e0ec2cfb31ae"},
    {"/system/bin/x2d-multi-resolution/merge16","d1a5a9a8a8584be82adf7d660bd7e038d3eb5fe0a71a4e7af979f494745545fd"},
    {"/system/bin/x2d-multi-resolution/render16","038a548e42b1b1be37d609620d7e712011ec8956c4226e2ac149d36ea0424b47"},
    {"/system/bin/x2d-multi-resolution/jpeg-grid","04fbefc63c1f24621cf745e3af0b28b80d48dc53bedd75d29bcbe9a19b22094a"},
    {"/system/bin/x2d-multi-resolution/pack16","a2d9fd24dfacec778e5753fad8a26073458e6353ebf39054281838f8836be9d2"},
    {"/system/bin/x2d-multi-resolution/libx2d_private_capture.so","d29606f07e88fbb7dde30217e857737ffa51e7ba3c0a47a540e580e7a7181204"},
    {"/system/bin/x2d-raw-stack/stack-pack","9f7ce45aa0b937d26a0bf4cef0abca811924fbc76af018325ad6c1dc4358b710"},
    {"/system/bin/x2d-raw-stack/finalize","3c86b4eab2dcf890ee10f911a9a3c55be8f810f39ebb60dba47acadbccea37f7"},
};
static constexpr const char* pixel_inputs_sha="aed57a4a9f35e3d3fd90f895613b0c7ab207c6b2f767f5bce6d0be8e0f8f9564";
static constexpr const char* pixel_fix_3fr_sha="2d0e587626dcd5f5a39d978b0a3a1dfa3b63f898062fbc1a33a3b1847812dd4d";
} }
