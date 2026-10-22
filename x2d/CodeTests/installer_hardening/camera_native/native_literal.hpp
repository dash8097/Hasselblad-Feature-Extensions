#pragma once
// 仅增加静态字符串恢复成本；运行时诊断保持原文，不是加密保密或抗调试边界。
#include <cstddef>
#include <cstdint>
namespace cn_literal {
constexpr uint64_t mix(uint64_t value) {
    value ^= value >> 30; value *= 0xbf58476d1ce4e5b9ULL;
    value ^= value >> 27; value *= 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}
template<size_t N,uint64_t Seed> struct Cipher {
    unsigned char data[N]{};
    constexpr Cipher(const char (&text)[N]) {
        for(size_t i=0;i<N;++i)data[i]=static_cast<unsigned char>(text[i])^static_cast<unsigned char>(mix(Seed+i));
    }
};
__attribute__((noinline)) inline void decode(char*target,const unsigned char*source,size_t n,uint64_t seed) {
    // volatile 防止优化器在编译期重建明文字节；无设备识别、反调试或外部密钥。
    volatile uint64_t runtime_seed=seed;
    for(size_t i=0;i<n;++i)target[i]=static_cast<char>(source[i]^static_cast<unsigned char>(mix(runtime_seed+i)));
}
template<size_t N,uint64_t Seed> struct Plain {
    char data[N]{};
    explicit Plain(const Cipher<N,Seed>&cipher){decode(data,cipher.data,N,Seed);}
};
}
#define CN_HIDDEN_LITERAL_IMPL(text,id) ([]()->const char* { \
    constexpr uint64_t key=cn_literal::mix(CN_LITERAL_SEED+static_cast<uint64_t>(id)); \
    constexpr cn_literal::Cipher<sizeof(text),key> cipher(text); \
    static const cn_literal::Plain<sizeof(text),key> plain(cipher); \
    return plain.data; }())
#define CN_HIDDEN_LITERAL(text) CN_HIDDEN_LITERAL_IMPL(text,__COUNTER__)
