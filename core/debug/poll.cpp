#include "ds_system.h"
#include <cstdio>
#include <fstream>
#include <iterator>
using namespace nds;
int main(int argc, char** argv) {
    std::ifstream f(argv[1], std::ios::binary); std::vector<uint8_t> rom((std::istreambuf_iterator<char>(f)), {});
    static Console c; c.load_rom(rom); c.run_units(atoll(argv[2]));
    for (int k = 0; k < 6; k++) { c.run_units(atoll(argv[3]));
        std::printf("t+%d: ARM9 pc=%08x  [02132e28]=%08x [02132e24]=%08x  cart B7=%llu  frame=%llu\n", k, c.cpu9.pc(), c.bus9.read32(0x02132e28), c.bus9.read32(0x02132e24), (unsigned long long)c.card_cmd_count[0xB7], (unsigned long long)c.video_frames); }
}
