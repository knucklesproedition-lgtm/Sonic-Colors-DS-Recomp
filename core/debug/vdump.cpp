#include "ds_system.h"
#include <cstdio>
#include <fstream>
#include <iterator>
using namespace nds;
int main(int argc, char** argv) {
    std::ifstream f(argv[1], std::ios::binary); std::vector<uint8_t> rom((std::istreambuf_iterator<char>(f)), {});
    static Console c; c.load_rom(rom); c.run_units(atoll(argv[2]));
    auto r16 = [&](uint32_t a) { return c.bus9.read16(a); };
    for (int e = 0; e < 2; e++) { uint32_t b = 0x04000000 + (e ? 0x1000 : 0);
        std::printf("== motor %c ==  DISPCNT=%08x  BGCNT:", e ? 'B' : 'A', c.bus9.read32(b)); for (int i = 0; i < 4; i++) std::printf(" %04x", r16(b + 8 + 2 * i)); std::printf("\n");
        std::printf("   BG2: PA=%04x PB=%04x PC=%04x PD=%04x X=%08x Y=%08x   BG3: PA=%04x PD=%04x\n", r16(b + 0x20), r16(b + 0x22), r16(b + 0x24), r16(b + 0x26), c.bus9.read32(b + 0x28), c.bus9.read32(b + 0x2C), r16(b + 0x30), r16(b + 0x36));
        std::printf("   BLDCNT=%04x BLDALPHA=%04x BLDY=%04x MOSAIC=%04x WIN0H=%04x WININ=%04x  MASTER_BRIGHT=%04x\n", r16(b + 0x50), r16(b + 0x52), r16(b + 0x54), r16(b + 0x4C), r16(b + 0x40), r16(b + 0x48), r16(b + 0x6C)); }
    std::printf("VRAMCNT A-G:"); for (int i = 0; i < 7; i++) std::printf(" %02x", c.bus9.read8(0x04000240 + i)); std::printf(" H:%02x I:%02x  POWCNT1=%04x\n", c.bus9.read8(0x04000248), c.bus9.read8(0x04000249), r16(0x04000304));
    for (int b = 0; b < 4; b++) { long nz = 0; for (int i = 0; i < 0x20000; i++) nz += c.vram[b * 0x20000 + i] != 0; std::printf("banco %c: %ld bytes no nulos de 131072\n", 'A' + b, nz); }
    std::printf("DISP3DCNT=%04x  GXSTAT=%04x%04x\n", r16(0x04000060), r16(0x04000602), r16(0x04000600));
}
