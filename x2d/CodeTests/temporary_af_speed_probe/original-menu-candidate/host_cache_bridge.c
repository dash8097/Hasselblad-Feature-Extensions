/* Desktop-only callback: avoids holding Python GIL across Qt's loader thread. */
typedef unsigned char (*equals_fn)(const void *, const void *);
static equals_fn eq;
static const void *url, *unit;
static unsigned hits;
__declspec(dllexport) void configure(equals_fn comparator, const void *expected, const void *cached) {
    eq = comparator; url = expected; unit = cached; hits = 0;
}
__declspec(dllexport) const void *lookup(const void *actual) {
    if (eq(actual, url)) { ++hits; return unit; }
    return 0;
}
__declspec(dllexport) unsigned hit_count(void) { return hits; }
