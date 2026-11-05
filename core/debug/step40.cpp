#include "ds_system.h"
#include <cstdio>
#include <fstream>
#include <iterator>
using namespace nds;
int main(int argc, char** argv) {
    std::ifstream f(argv[1], std::ios::binary); std::vector<uint8_t> rom((std::istreambuf_iterator<char>(f)), {});
    static Console c; c.load_rom(rom); c.run_units(atoll(argv[2]));
    for (int i = 0; i < atoi(argv[3]); i++) { uint32_t pc = c.cpu9.pc();
        std::printf("%08x %08x r0=%08x r1=%08x r7=%08x r8=%08x r9=%08x\n", pc, c.bus9.read32(pc & ~3u), c.cpu9.r[0], c.cpu9.r[1], c.cpu9.r[7], c.cpu9.r[8], c.cpu9.r[9]); c.cpu9.step(); }
}
