#include "../native/preview_cache.h"
extern "C" {
#include "../native/display_pixels.h"
}
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using Pixels = std::shared_ptr<std::vector<uint8_t>>;
using Cache = X1D::PreviewCache<std::string, Pixels>;
static unsigned checks = 0;
static void require(bool value, const char *what) {
    ++checks;
    if (!value) throw std::runtime_error(what);
}
static double millis(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}
static void fill(std::vector<uint8_t> &p, unsigned seed) {
    for (size_t i = 0; i < p.size(); ++i) p[i] = uint8_t((i * 37 + seed * 71) ^ (i >> 8));
}
static uint64_t hash(const std::vector<uint8_t> &p) {
    uint64_t h = 1469598103934665603ull;
    for (uint8_t b : p) h = (h ^ b) * 1099511628211ull;
    return h;
}
static Pixels transformed(unsigned seed, unsigned w = 1108, unsigned h = 830) {
    Pixels p(new std::vector<uint8_t>(size_t(w) * h * 4));
    fill(*p, seed);
    std::vector<uint8_t> scratch(xj_orient_scratch_bytes(w,h,6));
    if (xj_display_bgra(p->data(), p->size(), w, h, 1) ||
        xj_orient_bgra(p->data(), p->size(), w, h, 6, scratch.data(), scratch.size()))
        throw std::runtime_error("candidate pixel transform failed");
    return p;
}

static void correctness() {
    Cache c;
    Pixels out;
    require(!c.get("missing", &out) && !out, "empty hit");
    Pixels a(new std::vector<uint8_t>(12, 1));
    Pixels b(new std::vector<uint8_t>(12, 2));
    Pixels d(new std::vector<uint8_t>(12, 3));
    require(!c.put("a", a, 0), "zero budget accepted");
    require(!c.put("a", a, Cache::MaximumBytes + size_t(1)), "oversize accepted");
    require(c.put("a", a, 12) && c.bytes() == 12 && c.entries() == 1, "first entry");
    require(!c.get("a", nullptr), "null output accepted");
    require(c.get("a", &out) && out.get() == a.get(), "hit copied pixels");
    require(!c.get("different-prefix", &out), "different revision reused");
    require(c.put("b", b, 12), "second entry");
    require(c.get("a", &out), "promote first");
    require(c.put("d", d, 12), "replace LRU");
    require(c.entries() == 2 && c.bytes() == 24, "entry bound");
    require(!c.get("b", &out) && c.get("a", &out), "LRU ordering");
    require(c.put("a", d, 16) && c.bytes() == 28, "same key accounting");
    require(c.get("a", &out) && out.get() == d.get(), "same key not replaced");
    c.clear();
    require(c.entries() == 0 && c.bytes() == 0, "clear accounting");
    require(c.put("large-a", a, Cache::MaximumBytes - 1), "large entry");
    require(c.put("large-b", b, 2), "byte budget insertion");
    require(c.entries() == 1 && c.bytes() == 2 && !c.get("large-a", &out), "byte budget eviction");
    c.clear();
    std::weak_ptr<std::vector<uint8_t>> weak;
    {
        Pixels owned(new std::vector<uint8_t>(32, 42));
        weak = owned;
        require(c.put("shared-owner", owned, 32), "shared owner insertion");
        require(c.get("shared-owner", &out), "shared owner get");
    }
    c.clear();
    require(!weak.expired() && out->at(0) == 42, "eviction invalidated displayed image");
    out.reset();
    require(weak.expired(), "last reference leaked");
    std::weak_ptr<std::vector<uint8_t>> dying;
    {
        Cache bounded;
        Pixels owned(new std::vector<uint8_t>(32, 43));
        dying = owned;
        require(bounded.put("destructor", owned, 32), "destructor insertion");
    }
    require(dying.expired(), "cache destructor leaked");

    Cache actual;
    Pixels p0 = transformed(0), p1 = transformed(1);
    const size_t cost = 1108u * 830u * 4u;
    require(actual.put("prefix-hash-0", p0, cost) && actual.put("prefix-hash-1", p1, cost), "real preview insertion");
    require(actual.bytes() == 7357120u && actual.bytes() <= Cache::MaximumBytes, "real preview budget");
    require(actual.get("prefix-hash-0", &out) && hash(*out) == hash(*transformed(0)), "cached pixels changed");
    require(actual.get("prefix-hash-1", &out) && hash(*out) == hash(*transformed(1)), "second cached pixels changed");
}

static double median(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    return values.at(values.size() / 2);
}

static void benchmark() {
    std::vector<double> oldTimes, newTimes;
    const uint64_t wanted[] = {hash(*transformed(0)), hash(*transformed(1))};
    for (unsigned trial = 0; trial < 5; ++trial) {
        for (unsigned useCache = 0; useCache < 2; ++useCache) {
            Cache cache;
            unsigned transforms = 0;
            double elapsed = 0;
            for (unsigned i = 0; i < 8; ++i) {
                const unsigned id = i % 2;
                const std::string key = id ? "verified-prefix-B" : "verified-prefix-A";
                const auto start = std::chrono::steady_clock::now();
                Pixels result;
                if (!useCache || !cache.get(key, &result)) {
                    result = transformed(id);
                    ++transforms;
                    if (useCache) cache.put(key, result, result->size());
                }
                elapsed += millis(start);
                require(hash(*result) == wanted[id], "sequence pixel mismatch");
            }
            require(transforms == (useCache ? 2u : 8u), "transform count");
            (useCache ? newTimes : oldTimes).push_back(elapsed);
        }
    }
    std::vector<double> color, orient;
    const unsigned w = 8176, h = 6128;
    std::vector<uint8_t> full(size_t(w) * h * 4), scratch(xj_orient_scratch_bytes(w,h,6));
    for (unsigned trial = 0; trial < 3; ++trial) {
        fill(full, trial);
        auto start = std::chrono::steady_clock::now();
        require(xj_display_bgra(full.data(), full.size(), w, h, 1) == 0, "full color failed");
        color.push_back(millis(start));
        start = std::chrono::steady_clock::now();
        require(xj_orient_bgra(full.data(), full.size(), w, h, 6, scratch.data(), scratch.size()) == 0, "full orient failed");
        orient.push_back(millis(start));
    }
    std::printf("{\"passed\":true,\"checks\":%u,\"hardwareRequests\":0,\"targetRuntimeRun\":false,"
                "\"previewRequestsPerTrial\":8,\"uncachedTransforms\":8,\"cachedTransforms\":2,"
                "\"retainedPreviewBytes\":7357120,\"previewTrials\":5,"
                "\"hostPreviewUncachedMedianMs\":%.3f,\"hostPreviewCachedMedianMs\":%.3f,"
                "\"hostFullAdobeMedianMs\":%.3f,\"hostFullOrientation6MedianMs\":%.3f,"
                "\"fullCpuPixelBytes\":200410112,\"fullOrientationScratchBytes\":%zu}\n",
                checks, median(oldTimes), median(newTimes), median(color), median(orient), scratch.size());
}

int main() {
    try { correctness(); benchmark(); return 0; }
    catch (const std::exception &e) { std::fprintf(stderr, "check %u failed: %s\n", checks, e.what()); return 1; }
}
