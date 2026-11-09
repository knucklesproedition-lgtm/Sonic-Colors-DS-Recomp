// API C minima para probar el interprete desde Python (ctypes). Solo para tests.
#include "arm_cpu.h"
#include <cstring>
using namespace nds;

struct TBus : Bus {
    static const uint32_t CODE = 0x02000000, DATA = 0x02EF0000;
    uint8_t code[0x1000] = {}; uint8_t data[0x4000] = {}; bool err = false;
    uint8_t* at(uint32_t a, int n) {
        if (a >= CODE && (uint64_t)a + n <= (uint64_t)CODE + sizeof code) return code + (a - CODE);
        if (a >= DATA && (uint64_t)a + n <= (uint64_t)DATA + sizeof data) return data + (a - DATA);
        err = true; return nullptr;
    }
    uint32_t rd(uint32_t a, int n) { uint8_t* p = at(a, n); uint32_t v = 0; if (p) std::memcpy(&v, p, n); return v; }
    void wr(uint32_t a, uint32_t v, int n) { uint8_t* p = at(a, n); if (p) std::memcpy(p, &v, n); }
    uint32_t read32(uint32_t a) override { return rd(a, 4); }
    uint32_t read16(uint32_t a) override { return rd(a, 2); }
    uint32_t read8(uint32_t a) override { return rd(a, 1); }
    void write32(uint32_t a, uint32_t v) override { wr(a, v, 4); }
    void write16(uint32_t a, uint32_t v) override { wr(a, v, 2); }
    void write8(uint32_t a, uint32_t v) override { wr(a, v, 1); }
    bool swi(Cpu&, uint32_t) override { err = true; return true; }   // los tests no cubren SWI
    uint32_t cp15_read(uint32_t, uint32_t, uint32_t, uint32_t) override { err = true; return 0; }
    void cp15_write(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) override { err = true; }
};
struct H { TBus bus; Cpu cpu; explicit H(int arch) : cpu(arch ? Cpu::ARM9_V5TE : Cpu::ARM7_V4T, &bus) {} };

extern "C" {
void* t_new(int arch) { return new H(arch); }
void t_free(void* h) { delete (H*)h; }
void t_load(void* h, const uint8_t* code, int ncode, const uint8_t* data) {
    H* x = (H*)h; std::memset(x->bus.code, 0, sizeof x->bus.code);
    std::memcpy(x->bus.code, code, ncode); std::memcpy(x->bus.data, data, sizeof x->bus.data); x->bus.err = false; x->cpu.unaligned_seen = false;
}
void t_set(void* h, const uint32_t* regs, uint32_t cpsr) { ((H*)h)->cpu.set_state(regs, cpsr); }
int t_run(void* h, int n) { H* x = (H*)h; for (int k = 0; k < n; k++) x->cpu.step(); return x->bus.err; }
void t_get(void* h, uint32_t* regs, uint32_t* cpsr) {
    H* x = (H*)h; for (int k = 0; k < 15; k++) regs[k] = x->cpu.r[k];
    regs[15] = x->cpu.pc(); *cpsr = x->cpu.cpsr();
}
int t_unaligned(void* h) { return ((H*)h)->cpu.unaligned_seen; }
const uint8_t* t_data(void* h) { return ((H*)h)->bus.data; }
}
