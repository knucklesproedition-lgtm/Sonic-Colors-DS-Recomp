#include "ds_system.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iterator>
using namespace nds;
int main(int argc, char** argv) {
    std::ifstream f(argv[1], std::ios::binary); std::vector<uint8_t> rom((std::istreambuf_iterator<char>(f)), {});
    static Console c; c.load_rom(rom); c.run_units(atoll(argv[2]));
    std::printf("lecturas del firmware por direccion de inicio:\n"); for (auto& kv : c.fw_reads) std::printf("  %06x x%llu\n", kv.first, (unsigned long long)kv.second);
    std::printf("transferencias SPI: energia=%llu firmware=%llu tactil=%llu\n", (unsigned long long)c.spi_transfers[0], (unsigned long long)c.spi_transfers[1], (unsigned long long)c.spi_transfers[2]);
}
