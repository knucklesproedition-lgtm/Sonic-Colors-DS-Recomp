#include "ds_system.h"
#include <cstdio>
#include <fstream>
#include <iterator>
using namespace nds;
int main(int argc, char** argv) {
    std::ifstream f(argv[1], std::ios::binary); std::vector<uint8_t> rom((std::istreambuf_iterator<char>(f)), {});
    static Console c; c.load_rom(rom); c.run_units(atoll(argv[2]));
    auto w = [&](uint32_t a) { return c.bus9.read32(a); };
    std::printf("[02132e24..30] = %08x %08x %08x %08x\n", w(0x02132e24), w(0x02132e28), w(0x02132e2c), w(0x02132e30));
    std::printf("[0213855c..70] (estado del DMA GX) = %08x %08x %08x %08x %08x %08x  [021069d8]=%08x\n", w(0x0213855c), w(0x02138560), w(0x02138564), w(0x02138568), w(0x0213856c), w(0x02138570), w(0x021069d8));
    for (int ch = 0; ch < 4; ch++) { auto& d = c.dma_state(0, ch); std::printf("DMA9 ch%d: sad=%08x dad=%08x cnt=%x ctrl=%04x enable=%d armed_card=%d icnt=%x\n", ch, d.sad, d.dad, d.cnt, d.ctrl, (d.ctrl >> 15) & 1, d.armed_card, d.icnt); }
    std::printf("IE=%08x IF=%08x IME=%u  GXmode=%u IRQ21 gen=%llu\n", c.IE[0], c.IF[0], c.IME[0], c.gxmode(), (unsigned long long)c.gx_raised);
    // funciones IRQ registradas por el SDK: tabla en DTCM (OS_IrqTable) ~ 0x027e3fc0-ish
    std::printf("tabla IRQ del SDK (DTCM+0x3F80 ..):"); for (uint32_t a = 0x027e3f80; a < 0x027e3fc0; a += 4) std::printf(" %08x", w(a)); std::printf("\n");
}
