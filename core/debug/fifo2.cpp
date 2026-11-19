#include "ds_system.h"
#include <cstdio>
#include <fstream>
#include <iterator>
using namespace nds;
int main(int argc, char** argv) {
    std::ifstream f(argv[1], std::ios::binary); std::vector<uint8_t> rom((std::istreambuf_iterator<char>(f)), {});
    static Console c; c.load_rom(rom); c.run_units(atoll(argv[2])); std::vector<std::pair<int,uint32_t>> log; c.fifo_log = &log;
    uint64_t span = atoll(argv[3]); c.run_units(span);
    std::printf("tras %s unidades: palabras FIFO en la ventana: %zu  | ARM9 pc=%08x ARM7 pc=%08x(h=%d)  IRQ9=%llu IRQ7=%llu\n", argv[2], log.size(), c.cpu9.pc(), c.cpu7.pc(), c.cpu7.halted, (unsigned long long)c.cpu9.irq_entries, (unsigned long long)c.cpu7.irq_entries);
    int n = (int)log.size(); int from = n > 24 ? n - 24 : 0;
    for (int i = from; i < n; i++) { uint32_t w = log[i].second; std::printf("  %s %08x tag=%02x err=%d dato=%07x\n", log[i].first ? "ARM7->ARM9" : "ARM9->ARM7", w, w & 31, (w >> 5) & 1, w >> 6); }
    std::printf("IE9=%08x IF9=%08x IME9=%u | IE7=%08x IF7=%08x IME7=%u  I7=%d\n", c.IE[0], c.IF[0], c.IME[0], c.IE[1], c.IF[1], c.IME[1], c.cpu7.I);
    for (int t = 0; t < 4; t++) { auto& T = c.timer(1, t); if (T.ctrl & 0x80) std::printf("  ARM7 timer%d ctrl=%04x reload=%04x cnt=%04x\n", t, T.ctrl, T.reload, T.counter); }
}
