// Rastro instruccion a instruccion del ARM9 desde la primera llamada a <addr> hasta que retorna.
#include "ds_system.h"
#include <cstdio>
#include <fstream>
#include <iterator>
using namespace nds;
int main(int argc, char** argv) {
    std::ifstream f(argv[1], std::ios::binary); std::vector<uint8_t> rom((std::istreambuf_iterator<char>(f)), {});
    uint32_t target = strtoul(argv[2], nullptr, 16); int maxn = argc > 3 ? atoi(argv[3]) : 4000;
    static Console c; c.load_rom(rom);
    uint32_t seen = 0; bool found = false;
    for (uint64_t u = 0; u < 6000000 && !found; u++) { c.run_units(1); while (seen < c.cpu9.ncalls) { if (c.cpu9.calls[seen & 511].to == target) found = true; seen++; if (found) break; } }
    if (!found) { std::puts("no se llamo"); return 1; }
    uint32_t ret = c.cpu9.r[14] & ~1u; int depth = 0; (void)depth;
    std::printf("# llamada a %08x desde lr=%08x\n", target, ret);
    for (int i = 0; i < maxn; i++) {
        uint32_t pc = c.cpu9.pc(); uint32_t op = c.bus9.read32(pc & ~3u);
        std::printf("%08x %08x r0=%08x r1=%08x r2=%08x r3=%08x\n", pc, op, c.cpu9.r[0], c.cpu9.r[1], c.cpu9.r[2], c.cpu9.r[3]);
        c.cpu9.step();
        if (c.cpu9.pc() == ret) { std::printf("# retorno a %08x con r0=%08x\n", ret, c.cpu9.r[0]); break; }
    }
}
