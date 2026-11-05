#include "ds_system.h"
#include <cstdio>
#include <fstream>
#include <iterator>
using namespace nds;
int main(int argc, char** argv) {
    std::ifstream f(argv[1], std::ios::binary); std::vector<uint8_t> rom((std::istreambuf_iterator<char>(f)), {});
    static Console c; c.load_rom(rom);
    uint64_t units = (argc > 2 ? atoll(argv[2]) : 5000000ull); c.run_units(units);
    for (int i = 0; i < 2; i++) {
        Cpu& p = i ? c.cpu7 : c.cpu9;
        std::printf("== %s == pc=%08x mode=%02x I=%d halted=%d irq_line=%d  IRQs=%llu SWIs=%llu\n", i ? "ARM7" : "ARM9", p.pc(), p.mode, p.I, p.halted, p.irq_line, (unsigned long long)p.irq_entries, (unsigned long long)p.swi_count);
        std::printf("   IME=%u IE=%08x IF=%08x DISPSTAT=%04x\n", c.IME[i], c.IE[i], c.IF[i], c.dispstat_(i));
        for (int t = 0; t < 4; t++) { auto& T = c.timer(i, t); if (T.ctrl & 0x80) std::printf("   timer%d: ctrl=%04x reload=%04x counter=%04x\n", t, T.ctrl, T.reload, T.counter); }
        std::printf("   FIFO: enviadas=%llu recibidas=%llu pendientes_en_mi_cola=%zu\n", (unsigned long long)c.fifo_pushed[i], (unsigned long long)c.fifo_popped[i], c.fifo_size(i));
        std::printf("   acceso a registros del cartucho:"); for (auto& kv : c.card_reg_access[i]) std::printf(" %05x x%llu", kv.first, (unsigned long long)kv.second); std::printf("\n");
    }
    std::printf("comandos de cartucho vistos:"); for (auto& kv : c.card_cmd_count) std::printf(" %02x x%llu", kv.first, (unsigned long long)kv.second); std::printf("\n");
}
