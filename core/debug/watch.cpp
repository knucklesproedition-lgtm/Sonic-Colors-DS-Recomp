#include "ds_system.h"
#include <cstdio>
#include <fstream>
#include <iterator>
using namespace nds;
int main(int argc, char** argv) {
    std::ifstream f(argv[1], std::ios::binary); std::vector<uint8_t> rom((std::istreambuf_iterator<char>(f)), {});
    uint32_t addr = strtoul(argv[2], nullptr, 16); uint64_t maxu = atoll(argv[3]); int maxprint = argc > 4 ? atoi(argv[4]) : 12;
    static Console c; c.load_rom(rom);
    uint32_t last = c.bus9.read32(addr); int printed = 0;
    for (uint64_t u = 0; u < maxu && printed < maxprint; u++) {
        c.run_units(1); uint32_t v = c.bus9.read32(addr);
        if (v != last) { std::printf("unidad %llu: [%08x] %08x -> %08x   ARM9 pc=%08x mode=%02x lr=%08x\n", (unsigned long long)u, addr, last, v, c.cpu9.pc(), c.cpu9.mode, c.cpu9.r[14]); last = v; printed++; }
    }
    std::printf("fin: [%08x]=%08x\n", addr, last);
}
