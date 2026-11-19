#include "ds_system.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iterator>
using namespace nds;
static void top(const char* t, std::map<uint32_t, uint64_t>& m, int n) { std::vector<std::pair<uint64_t, uint32_t>> v; for (auto& kv : m) v.push_back({kv.second, kv.first}); std::sort(v.rbegin(), v.rend());
    std::printf("%s\n", t); for (int i = 0; i < n && i < (int)v.size(); i++) std::printf("  %08x x%llu\n", v[i].second, (unsigned long long)v[i].first); }
int main(int argc, char** argv) {
    std::ifstream f(argv[1], std::ios::binary); std::vector<uint8_t> rom((std::istreambuf_iterator<char>(f)), {});
    static Console c; c.load_rom(rom); c.run_units(atoll(argv[2]));
    std::map<uint32_t, uint64_t> h9, h7, pcs, ior; c.cpu9.call_hist = &h9; c.cpu7.call_hist = &h7;
    for (int i = 0; i < 3000000; i++) { c.run_units(1); pcs[c.cpu9.pc() & ~0x3Fu]++; }
    top("ARM9 llamadas por 3M unidades (~5 fotogramas)", h9, 22); top("ARM9 bloques de 64 bytes mas ejecutados", pcs, 8); top("ARM7 llamadas", h7, 8);
    std::printf("IRQs ARM9=%llu ARM7=%llu  FIFO 9->7 %llu  7->9 %llu\n", (unsigned long long)c.cpu9.irq_entries, (unsigned long long)c.cpu7.irq_entries, (unsigned long long)c.fifo_pushed[0], (unsigned long long)c.fifo_pushed[1]);
}
