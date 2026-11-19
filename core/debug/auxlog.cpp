#include "ds_system.h"
#include <cstdio>
#include <fstream>
#include <iterator>
using namespace nds;
int main(int argc, char** argv) {
    std::ifstream f(argv[1], std::ios::binary); std::vector<uint8_t> rom((std::istreambuf_iterator<char>(f)), {});
    static Console c; c.load_rom(rom); std::vector<std::pair<uint16_t,uint8_t>> log; c.auxspi_log = &log;
    c.run_units(atoll(argv[2])); int n = argc > 3 ? atoi(argv[3]) : 80;
    std::printf("transferencias AUXSPI capturadas: %zu\n", log.size());
    for (int i = 0; i < n && i < (int)log.size(); i++) std::printf("%02x(%s) ", log[i].second, (log[i].first & 0x40) ? "hold" : "fin"); std::printf("\n");
}
