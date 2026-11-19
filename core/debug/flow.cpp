#include "ds_system.h"
#include <cstdio>
#include <fstream>
#include <iterator>
#include <set>
using namespace nds;
int main(int argc, char** argv) {
    std::ifstream f(argv[1], std::ios::binary); std::vector<uint8_t> rom((std::istreambuf_iterator<char>(f)), {});
    static Console c; c.load_rom(rom);
    std::vector<std::pair<uint32_t,uint32_t>> order; std::set<uint32_t> first; uint32_t prev_n = 0;
    for (uint64_t u = 0; u < 5300000; u++) {
        c.run_units(1);
        uint32_t pc = c.cpu9.pc();
        if (pc >= 0x02000000 && pc < 0x02400000 && pc >= 0x02100000) { uint32_t w = c.bus9.read32(pc & ~3u); if (!w && !c.bus9.read32((pc+4)&~3u) && !c.bus9.read32((pc+8)&~3u)) break; }
        uint32_t n = c.cpu9.ncalls;
        for (uint32_t k = prev_n; k < n && n - k < 400; k++) { auto& r = c.cpu9.calls[k & 511]; if (first.insert(r.to).second) order.push_back({r.from, r.to}); }
        prev_n = n;
    }
    std::printf("ARM9 termino en pc=%08x. Funciones en orden de PRIMERA llamada (llamador -> funcion): %zu\n", c.cpu9.pc(), order.size());
    int skip = 0; for (auto& o : order) { if (o.second == 0x020dcc50 || o.second == 0x020007a0) { skip++; } }
    for (size_t i = 0; i < order.size(); i++) std::printf("  %3zu: %08x -> %08x\n", i, order[i].first, order[i].second);
}
