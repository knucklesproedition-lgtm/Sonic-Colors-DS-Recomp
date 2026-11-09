#include "ds_system.h"
#include <cstdio>
#include <fstream>
#include <iterator>
using namespace nds;
int main(int argc, char** argv) {
    std::ifstream f(argv[1], std::ios::binary); std::vector<uint8_t> rom((std::istreambuf_iterator<char>(f)), {});
    static Console c; c.load_rom(rom); std::vector<std::pair<int,uint32_t>> log; c.fifo_log = &log;
    c.run_units(atoll(argv[2])); int skip = argc > 3 ? atoi(argv[3]) : 0; int n = argc > 4 ? atoi(argv[4]) : 60;
    std::printf("palabras de FIFO capturadas: %zu (mostrando desde %d)\n", log.size(), skip);
    for (int i = skip; i < skip + n && i < (int)log.size(); i++) { uint32_t w = log[i].second;
        std::printf("  %3d  %s  %08x   tag(bits0-4)=%02x err(bit5)=%d dato(bits6-31)=%07x\n", i, log[i].first ? "ARM7->ARM9" : "ARM9->ARM7", w, w & 31, (w >> 5) & 1, w >> 6); }
}
