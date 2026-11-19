// Ejecuta el arranque y, cuando el ARM9 sale de memoria con codigo, imprime las ultimas llamadas.
#include "ds_system.h"
#include <cstdio>
#include <fstream>
#include <iterator>
using namespace nds;
int main(int argc, char** argv) {
    std::ifstream f(argv[1], std::ios::binary); std::vector<uint8_t> rom((std::istreambuf_iterator<char>(f)), {});
    static Console c; c.load_rom(rom);
    uint64_t n = 0; uint32_t lastpc = 0;
    for (; n < 80000000; n++) {
        uint32_t pc = c.cpu9.pc();
        bool ok = (pc >= 0x02000000 && pc < 0x02400000) || pc >= 0xFFFF0000 || (pc < 0x02000000 && pc >= 0x01000000) || (pc >= 0x027C0000 && pc < 0x02800000);
        // memoria sin codigo: los 0x0219.. de overlays sin cargar son ceros
        if (ok && pc >= 0x02100000 && pc < 0x023FFFFF) { uint32_t w = c.bus9.read32(pc & ~3u); if (w == 0 && c.bus9.read32((pc + 4) & ~3u) == 0 && c.bus9.read32((pc + 8) & ~3u) == 0) ok = false; }
        if (!ok && n > 100) break;
        lastpc = pc; c.run_units(1);
    }
    std::printf("ARM9: codigo invalido en pc=%08x tras %llu unidades (ultimo pc valido %08x), lr=%08x\n", c.cpu9.pc(), (unsigned long long)n, lastpc, c.cpu9.r[14]);
    uint32_t total = c.cpu9.ncalls; int show = 70;
    std::printf("ultimas %d llamadas del ARM9 (desde -> hacia):\n", show);
    for (int i = show; i >= 1; i--) { auto& r = c.cpu9.calls[(total - i) & 511]; std::printf("  %08x -> %08x\n", r.from, r.to); }
    std::printf("\nARM7: pc=%08x halted=%d, llamadas=%u\n", c.cpu7.pc(), c.cpu7.halted, c.cpu7.ncalls);
}
