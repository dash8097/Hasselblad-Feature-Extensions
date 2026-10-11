#include "../native/godox_manual_power.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) return __LINE__; } while (0)
int main(int argc, char **argv)
{
    uint8_t out[4], before[4] = {0x12, 0x34, 0x56, 0x78};
    unsigned g, p, d, tenth, stops;
    if (argc == 2 && strcmp(argv[1], "--vectors") == 0) {
        for (g = 0; g < 5; ++g) for (p = 0; p <= 80; ++p) {
            CHECK(hbl_godox_manual_power_encode(out, g, p));
            CHECK(printf("%02x%02x%02x%02x\n", out[0], out[1], out[2], out[3]) == 9);
        }
        return 0;
    }
    CHECK(argc == 1);
    CHECK(hbl_godox_manual_power_from_fraction(out, 3, 32, 3));
    CHECK(memcmp(out, (uint8_t[]){0xa9, 0x0d, 0xbc, 47}, 4) == 0);
    for (stops = 0, d = 1; d <= 256; d <<= 1, ++stops)
        for (tenth = 0; tenth <= (stops ? 9u : 0u); ++tenth) {
            CHECK(hbl_godox_manual_power_from_fraction(out, 0, d, tenth));
            CHECK(out[3] == stops * 10 - tenth);
        }
    memcpy(out, before, 4);
    CHECK(!hbl_godox_manual_power_encode(out, 5, 0));
    CHECK(!hbl_godox_manual_power_encode(out, 0, 81));
    CHECK(!hbl_godox_manual_power_encode(out, 0, ~0u));
    CHECK(!hbl_godox_manual_power_encode(NULL, 0, 0));
    CHECK(!hbl_godox_manual_power_from_fraction(out, 0, 1, 1));
    CHECK(!hbl_godox_manual_power_from_fraction(out, 0, 32, 10));
    CHECK(!hbl_godox_manual_power_from_fraction(out, 0, 0, 0));
    CHECK(!hbl_godox_manual_power_from_fraction(out, 0, 512, 0));
    for (d = 1; d < 256; ++d) if (d & (d - 1))
        CHECK(!hbl_godox_manual_power_from_fraction(out, 0, d, 0));
    CHECK(memcmp(out, before, 4) == 0);
    puts("manual-power-encoding-passed");
    return 0;
}
