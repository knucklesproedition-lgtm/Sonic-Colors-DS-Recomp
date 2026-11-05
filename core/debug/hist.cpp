#include "ds_system.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <set>
using namespace nds;
int main(int argc, char** argv) {
    std::ifstream f(argv[1], std::ios::binary); std::vector<uint8_t> rom((std::istreambuf_iterator<char>(f)), {});
    static Console c; c.load_rom(rom);
    std::map<uint32_t, uint64_t> h9, h7; c.cpu9.call_hist = &h9; c.cpu7.call_hist = &h7;
    std::set<uint32_t> seen;
    for (uint64_t u = 0; u < 5000000; u++) { c.run_units(1); seen.insert(c.cpu9.pc() & ~3u); }
    auto top = [](const char* t, std::map<uint32_t, uint64_t>& m, int n) { std::vector<std::pair<uint64_t, uint32_t>> v; for (auto& kv : m) v.push_back({kv.second, kv.first}); std::sort(v.rbegin(), v.rend());
        std::printf("%s\n", t); for (int i = 0; i < n && i < (int)v.size(); i++) std::printf("  %08x x%llu\n", v[i].second, (unsigned long long)v[i].first); };
    top("ARM9: destinos de llamada mas frecuentes", h9, 24);
    std::printf("\nARM9: funciones de la zona CARD (0x020c8d00-0x020c9200) cuyo codigo SE EJECUTO:\n");
    uint32_t last = 0; for (uint32_t a : seen) if (a >= 0x020c8d00 && a < 0x020c9200 && (last == 0 || a - last > 0x40)) { std::printf("  desde %08x\n", a); last = a; } else if (a >= 0x020c8d00 && a < 0x020c9200) last = a;
    std::printf("(direcciones distintas ejecutadas en total: %zu)\n", seen.size());
    top("\nARM7: destinos de llamada mas frecuentes", h7, 10);
}
