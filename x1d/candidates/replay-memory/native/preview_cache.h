#ifndef X1D_PREVIEW_CACHE_H
#define X1D_PREVIEW_CACHE_H

#include <stddef.h>

namespace X1D {
/* 只供已完成源身份、前缀摘要和 EOI 核对的预览使用。调用方负责锁。
 * Value 必须是共享所有权的像素引用；逐出缓存不撤销仍显示的图像。
 * 预算约束缓存额外保留的像素，不冒称整个 Qt/CPU/GPU 的总存活上限。 */
template<class Key, class Value> class PreviewCache {
public:
    enum { MaximumEntries = 2, MaximumBytes = 8 * 1024 * 1024 };
    PreviewCache() : used_(0), bytes_(0) {}

    bool get(const Key &key, Value *value) {
        if (!value) return false;
        for (unsigned i = 0; i < used_; ++i) {
            if (!(entries_[i].key == key)) continue;
            const Entry hit = entries_[i];
            for (unsigned j = i; j > 0; --j) entries_[j] = entries_[j - 1];
            entries_[0] = hit;
            *value = hit.value;
            return true;
        }
        return false;
    }

    bool put(const Key &key, const Value &value, size_t cost) {
        if (!cost || cost > MaximumBytes) return false;
        const Entry next(key, value, cost);
        for (unsigned i = 0; i < used_; ++i)
            if (entries_[i].key == key) { remove(i); break; }
        while (used_ && (used_ == MaximumEntries || bytes_ > MaximumBytes - cost))
            remove(used_ - 1);
        for (unsigned i = used_; i > 0; --i) entries_[i] = entries_[i - 1];
        entries_[0] = next;
        ++used_;
        bytes_ += cost;
        return true;
    }

    void clear() { while (used_) remove(used_ - 1); }
    unsigned entries() const { return used_; }
    size_t bytes() const { return bytes_; }

private:
    struct Entry {
        Key key;
        Value value;
        size_t cost;
        Entry() : cost(0) {}
        Entry(const Key &k, const Value &v, size_t n) : key(k), value(v), cost(n) {}
    };
    void remove(unsigned index) {
        bytes_ -= entries_[index].cost;
        for (unsigned i = index + 1; i < used_; ++i) entries_[i - 1] = entries_[i];
        entries_[--used_] = Entry();
    }
    Entry entries_[MaximumEntries];
    unsigned used_;
    size_t bytes_;
};
}

#endif
