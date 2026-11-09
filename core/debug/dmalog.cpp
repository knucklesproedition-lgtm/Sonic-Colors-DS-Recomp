#include "ds_system.h"
#include <cstdio>
#include <fstream>
#include <iterator>
using namespace nds;
int main(int argc, char** argv) {
    std::ifstream f(argv[1], std::ios::binary); std::vector<uint8_t> rom((std::istreambuf_iterator<char>(f)), {});
    static Console c; c.load_rom(rom); c.run_units(atoll(argv[2]));
    std::printf("ultimos DMAs activados (de %llu):\n", (unsigned long long)c.dma_seq_);
    int n = (int)c.dma_log.size(); for (int i = 0; i < n; i++) { auto& e = c.dma_log[i]; if (e.dst != 0x04000400) continue;
        std::printf("  #%llu %s ch%d src=%08x dst=%08x cnt=%x ctrl=%04x  (timing=%d irq=%d rep=%d 32bit=%d) pc=%08x lr=%08x\n", (unsigned long long)e.seq, e.cpu ? "ARM7" : "ARM9", e.ch, e.src, e.dst, e.cnt, e.ctrl, e.cpu ? (e.ctrl >> 12) & 3 : (e.ctrl >> 11) & 7, (e.ctrl >> 14) & 1, (e.ctrl >> 9) & 1, (e.ctrl >> 10) & 1, e.pc, e.lr); }
    std::printf("GXSTAT: modo=%u escrituras=%llu ultimo=%04x  IRQ21 generadas=%llu\n", c.gxmode(), (unsigned long long)c.gx_stat_writes, c.gx_last_stat, (unsigned long long)c.gx_raised);
    std::printf("IF9 borrados (mascara x veces):"); for (auto& kv : c.if_clears[0]) std::printf(" %08x x%llu", kv.first, (unsigned long long)kv.second); std::printf("\n");
    std::printf("IE9=%08x IF9=%08x IME9=%u  ARM9 pc=%08x halted=%d I=%d\n", c.IE[0], c.IF[0], c.IME[0], c.cpu9.pc(), c.cpu9.halted, c.cpu9.I);
}
