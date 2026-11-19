#include "ds_system.h"
#include <cstdio>
#include <fstream>
#include <iterator>
using namespace nds;
int main(int argc, char** argv) {
    std::ifstream f(argv[1], std::ios::binary); std::vector<uint8_t> rom((std::istreambuf_iterator<char>(f)), {});
    static Console c; c.load_rom(rom); c.run_units(atoll(argv[2]));
    auto w = [&](uint32_t a) { return c.bus9.read32(a); };
    uint32_t l0 = 0, l1 = 0, l2 = 0, l3 = 0, lf = 0xFFFFFFFF; int n = 0;
    for (uint64_t u = 0; u < (uint64_t)atoll(argv[3]) && n < 24; u++) {
        c.run_units(1);
        uint32_t a = w(0x0213855c), b = w(0x02138560), s = w(0x02138564), len = w(0x02138568), fl = w(0x02132e28);
        static bool lp = false; if (a != l0 || s != l1 || len != l2 || fl != lf || b != l3 || c.gxpending() != lp) { lp = c.gxpending(); std::printf("u=%llu busy=%u dma=%u src=%08x len=%x flag=%u | IE=%08x IF=%08x GXmode=%u pend=%d gx_raised=%llu pc=%08x mode=%02x\n", (unsigned long long)u, a, b, s, len, fl, c.IE[0], c.IF[0], c.gxmode(), (int)c.gxpending(), (unsigned long long)c.gx_raised, c.cpu9.pc(), c.cpu9.mode); l0 = a; l1 = s; l2 = len; lf = fl; l3 = b; n++; }
    }
}
