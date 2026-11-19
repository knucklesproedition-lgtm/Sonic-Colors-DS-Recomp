#include "ds_system.h"
#include <cstdio>
#include <fstream>
#include <iterator>
using namespace nds;
int main(int argc, char** argv) {
    std::ifstream f(argv[1], std::ios::binary); std::vector<uint8_t> rom((std::istreambuf_iterator<char>(f)), {});
    static Console c; c.load_rom(rom); c.run_units(atoll(argv[2]));
    std::printf("ultimas 40 llamadas del ARM9 (desde -> hacia):\n");
    uint32_t t = c.cpu9.ncalls; for (int i = 40; i >= 1; i--) { auto& r = c.cpu9.calls[(t - i) & 511]; std::printf("  %08x -> %08x\n", r.from, r.to); }
}
