// Muestra que capas/modos usa el juego en un instante: g++ ... debug/layers.cpp ; ./layers rom unidades "pulsaciones"
#include "ds_system.h"
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <tuple>
using namespace nds;
int main(int argc, char** argv) {
    std::ifstream f(argv[1], std::ios::binary); std::vector<uint8_t> rom((std::istreambuf_iterator<char>(f)), {});
    static Console c; c.load_rom(rom); c.set_graphics(1, 0); uint64_t units = atoll(argv[2]) * 1000000ull;
    std::vector<std::tuple<uint64_t,uint64_t,uint32_t>> presses; if (argc > 3) { std::string a = argv[3]; size_t p = 0; while (p < a.size()) { size_t e = a.find(',', p); if (e == std::string::npos) e = a.size(); unsigned t, d, k; if (std::sscanf(a.substr(p, e - p).c_str(), "%u:%u:%x", &t, &d, &k) == 3) presses.push_back({t * 1000000ull, d * 1000000ull, k}); p = e + 1; } }
    uint64_t done = 0; while (done < units) { c.keys = 0; for (auto& pr : presses) if (done >= std::get<0>(pr) && done < std::get<0>(pr) + std::get<1>(pr)) c.keys |= std::get<2>(pr); c.run_units(2000000); done += 2000000; }
    for (int e = 0; e < 2; e++) { uint32_t b = 0x04000000 + (e ? 0x1000 : 0); uint32_t d = c.bus9.read32(b);
        std::printf("motor %c DISPCNT=%08x modo BG=%u  BG0..3 on=%d%d%d%d OBJ=%d  3D=%d\n", e ? 'B' : 'A', d, d & 7, (d >> 8) & 1, (d >> 9) & 1, (d >> 10) & 1, (d >> 11) & 1, (d >> 12) & 1, (d >> 3) & 1);
        for (int i = 0; i < 4; i++) std::printf("   BG%d CNT=%04x prio=%u 256col=%d bit2=%d tam=%u hof=%u vof=%u", i, c.bus9.read16(b + 8 + 2 * i), c.bus9.read16(b + 8 + 2 * i) & 3, (c.bus9.read16(b + 8 + 2 * i) >> 7) & 1, (c.bus9.read16(b + 8 + 2 * i) >> 2) & 1, c.bus9.read16(b + 8 + 2 * i) >> 14, c.bus9.read16(b + 0x10 + 4 * i) & 0x1FF, c.bus9.read16(b + 0x12 + 4 * i) & 0x1FF), i >= 2 ? std::printf("  PA=%04x PD=%04x X=%08x Y=%08x\n", c.bus9.read16(b + 0x20 + 0x10 * (i - 2)), c.bus9.read16(b + 0x26 + 0x10 * (i - 2)), c.bus9.read32(b + 0x28 + 0x10 * (i - 2)), c.bus9.read32(b + 0x2C + 0x10 * (i - 2))) : std::printf("\n");
        std::printf("   BLDCNT=%04x BLDALPHA=%04x WININ=%04x WINOUT=%04x WIN0H=%04x MOSAIC=%04x\n", c.bus9.read16(b + 0x50), c.bus9.read16(b + 0x52), c.bus9.read16(b + 0x48), c.bus9.read16(b + 0x4A), c.bus9.read16(b + 0x40), c.bus9.read16(b + 0x4C)); }
    std::printf("VRAMCNT A-G: "); for (int i = 0; i < 7; i++) std::printf("%02x ", c.bus9.read8(0x04000240 + i)); std::printf("H=%02x I=%02x  DISP3DCNT=%04x DISPCAPCNT=%08x\n", c.bus9.read8(0x04000248), c.bus9.read8(0x04000249), c.bus9.read16(0x04000060), c.bus9.read32(0x04000064));
    std::printf("3D: frames=%llu polys=%llu texturizados=%llu culled=%llu cmds_desconocidos=%llu (ultimo %02x)\n", (unsigned long long)c.gpu3d.frames, (unsigned long long)c.gpu3d.polys_drawn, (unsigned long long)c.gpu3d.textured_polys, (unsigned long long)c.gpu3d.polys_culled, (unsigned long long)c.gpu3d.unknown_cmds, c.gpu3d.last_unknown_cmd);
}
