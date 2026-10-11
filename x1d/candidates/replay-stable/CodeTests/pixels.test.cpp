#include "../native/display_pixels.h"
extern "C" {
int reference_display_bgra(uint8_t *, uint32_t, uint32_t, uint32_t, int);
int reference_orient_bgra(uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t, uint8_t *, uint32_t);
}
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>

static unsigned checks = 0, orientations = 0;
static void require(bool value, const char *what) {
    ++checks;
    if (!value) throw std::runtime_error(what);
}
static void fill(std::vector<uint8_t> &v, uint32_t seed) {
    for (size_t i = 0; i < v.size(); ++i) {
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        v[i] = uint8_t(seed);
    }
}
static double ms(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}
static double median(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    return values.at(values.size() / 2);
}

static void all_colors() {
    // 分批覆盖完整 24-bit RGB 空间；包含区间表所有不确定输入，不采用容差。
    std::vector<uint8_t> a(256 * 256 * 4), b(a.size());
    for (unsigned r = 0; r < 256; ++r) {
        for (unsigned g = 0; g < 256; ++g) for (unsigned blue = 0; blue < 256; ++blue) {
            const unsigned i = ((g << 8) | blue) * 4;
            a[i] = blue; a[i + 1] = g; a[i + 2] = r; a[i + 3] = uint8_t(r ^ g ^ blue);
        }
        b = a;
        require(reference_display_bgra(a.data(), a.size(), 256, 256, 1) == 0, "reference color rejected");
        require(xj_display_bgra(b.data(), b.size(), 256, 256, 1) == 0, "candidate color rejected");
        require(a == b, "24-bit RGB output differs from fixed v3 integer transform");
    }
}

static void all_directions() {
    const unsigned dimensions[][2] = {
        {1,1}, {1,7}, {6,1}, {2,2}, {8,8}, {15,9}, {16,16}, {32,48},
        {64,80}, {96,144}, {256,320}, {512,512}, {528,528}, {576,640},
        {1108,830}, {8176,16}, {16,8176}
    };
    for (const auto &dim : dimensions) {
        const unsigned w = dim[0], h = dim[1], bytes = w * h * 4;
        std::vector<uint8_t> source(bytes + 2);
        fill(source, w * 331 + h);
        for (unsigned o = 1; o <= 8; ++o) {
            std::vector<uint8_t> a(source), b(source), oldScratch((w * h + 7) / 8);
            const unsigned need = xj_orient_scratch_bytes(w, h, o);
            std::vector<uint8_t> scratch(need + 2, 0x5a);
            require(need <= oldScratch.size(), "scratch exceeds old memory budget");
            require(reference_orient_bgra(a.data() + 1, bytes, w, h, o, oldScratch.data(), oldScratch.size()) == 0, "reference direction rejected");
            require(xj_orient_bgra(b.data() + 1, bytes, w, h, o, need ? scratch.data() + 1 : nullptr, need) == 0, "candidate direction rejected");
            require(a == b, "direction or unaligned image guard mismatch");
            require(scratch.front() == 0x5a && scratch.back() == 0x5a, "scratch guard overwritten");
            ++orientations;
            if (need) {
                b = source;
                require(xj_orient_bgra(b.data() + 1, bytes, w, h, o, scratch.data() + 1, need - 1) == XJ_CAPACITY, "small scratch accepted");
                require(b == source, "failed transform changed image");
                require(xj_orient_bgra(b.data() + 1, bytes, w, h, o, b.data() + 1, need) == XJ_OVERLAP, "overlap accepted");
                require(b == source, "overlap failure changed image");
            }
        }
    }
    uint8_t one[4] = {1,2,3,4};
    require(xj_orient_bgra(one,4,1,1,9,nullptr,0) == XJ_ARGUMENT, "bad orientation accepted");
    require(xj_orient_bgra(one,3,1,1,1,nullptr,0) == XJ_ARGUMENT, "bad byte count accepted");
    require(xj_display_bgra(one,4,1,1,2) == XJ_ARGUMENT, "bad color profile accepted");
    require(xj_display_bgra(one,4,1,1,0) == 0 && one[0] == 1 && one[1] == 2 && one[2] == 3 && one[3] == 255, "sRGB changed colors");
}

static void full_size() {
    const unsigned w = 8176, h = 6128, bytes = w * h * 4;
    std::vector<uint8_t> a(bytes), b(bytes), oldScratch((w * h + 7) / 8);
    const unsigned need = xj_orient_scratch_bytes(w,h,6);
    std::vector<uint8_t> scratch(need);
    std::vector<double> oldColor, newColor, oldOrient, newOrient;
    for (unsigned trial = 0; trial < 3; ++trial) {
        fill(a, trial + 234567); b = a;
        auto start = std::chrono::steady_clock::now();
        const int ca = reference_display_bgra(a.data(),bytes,w,h,1);
        oldColor.push_back(ms(start));
        start = std::chrono::steady_clock::now();
        const int cb = xj_display_bgra(b.data(),bytes,w,h,1);
        newColor.push_back(ms(start));
        require(ca == 0 && cb == 0 && a == b, "full color differs");
        start = std::chrono::steady_clock::now();
        const int oa = reference_orient_bgra(a.data(),bytes,w,h,6,oldScratch.data(),oldScratch.size());
        oldOrient.push_back(ms(start));
        start = std::chrono::steady_clock::now();
        const int ob = xj_orient_bgra(b.data(),bytes,w,h,6,scratch.data(),scratch.size());
        newOrient.push_back(ms(start));
        require(oa == 0 && ob == 0 && a == b, "full direction differs");
    }
    std::printf("{\"passed\":true,\"checks\":%u,\"allRgbColors\":16777216,\"orientationCases\":%u,"
                "\"fullTrials\":3,\"fullByteIdentical\":true,\"fullCpuPixelBytes\":%u,"
                "\"oldOrientationScratchBytes\":%zu,\"newOrientationScratchBytes\":%u,\"tileStackBytes\":2048,"
                "\"hostV3AdobeMedianMs\":%.3f,\"hostNextAdobeMedianMs\":%.3f,"
                "\"hostV3Orientation6MedianMs\":%.3f,\"hostNextOrientation6MedianMs\":%.3f}\n",
                checks, orientations, bytes, oldScratch.size(), need,
                median(oldColor),median(newColor),median(oldOrient),median(newOrient));
}

int main() {
    try { all_colors(); all_directions(); full_size(); return 0; }
    catch (const std::exception &error) { std::fprintf(stderr,"check %u: %s\n",checks,error.what()); return 1; }
}
