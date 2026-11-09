#include "ds_system.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iterator>
using namespace nds;
int main(int argc, char** argv) {
    std::ifstream f(argv[1], std::ios::binary); std::vector<uint8_t> rom((std::istreambuf_iterator<char>(f)), {});
    static Console c; c.load_rom(rom); c.run_units(atoll(argv[2]));
    std::map<uint32_t, uint64_t> h; c.rd_log[0] = &h; c.run_units(400000); c.rd_log[0] = nullptr;
    std::vector<std::pair<uint64_t, uint32_t>> v; for (auto& kv : h) v.push_back({kv.second, kv.first}); std::sort(v.rbegin(), v.rend());
    std::printf("ARM9 pc=%08x. Direcciones mas leidas en 400k unidades:\n", c.cpu9.pc());
    for (size_t i = 0; i < v.size() && i < 14; i++) std::printf("  %08x x%llu  = %08x\n", v[i].second, (unsigned long long)v[i].first, c.bus9.read32(v[i].second));
}
