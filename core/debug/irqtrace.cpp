#include "ds_system.h"
#include <cstdio>
#include <fstream>
#include <iterator>
using namespace nds;
int main(int argc, char** argv) {
    std::ifstream f(argv[1], std::ios::binary); std::vector<uint8_t> rom((std::istreambuf_iterator<char>(f)), {});
    static Console c; c.load_rom(rom); c.run_units(atoll(argv[2]));
    uint32_t want = strtoul(argv[3], nullptr, 16); int maxn = atoi(argv[4]);
    // esperar a que el ARM9 entre en modo IRQ con la mascara pedida pendiente
    bool found = false;
    for (uint64_t u = 0; u < (argc > 5 ? (uint64_t)atoll(argv[5]) : 20000000ull) && !found; u++) { uint32_t pif = c.IF[0]; c.run_units(1); if (c.cpu9.mode == 0x12 && (pif & want & c.IE[0])) found = true; }
    if (!found) { std::puts("no se vio la IRQ"); return 1; }
    std::printf("# IRQ entrada: IF=%08x IE=%08x pc=%08x\n", c.IF[0], c.IE[0], c.cpu9.pc());
    for (int i = 0; i < maxn; i++) { uint32_t pc = c.cpu9.pc(); std::printf("%08x %08x r0=%08x r1=%08x r2=%08x\n", pc, c.bus9.read32(pc & ~3u), c.cpu9.r[0], c.cpu9.r[1], c.cpu9.r[2]); c.cpu9.step(); if (c.cpu9.mode != 0x12 && c.cpu9.mode != 0x1f) {} if (c.cpu9.mode == 0x1f && i > 20) break; }
}
