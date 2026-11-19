// arm_cpu.h - Interprete ARM/Thumb para las dos CPU de la DS:
//   ARM9 = ARM946E-S (ARMv5TE: CLZ, BLX, QADD, SMULxy, LDRD/STRD, interworking en LDR/LDM a PC)
//   ARM7 = ARM7TDMI  (ARMv4T)
// Sirve para: arrancar el juego, ejecutar codigo que el recomp no puede traducir (Thumb, saltos
// indirectos, overlays) y ejecutar funciones parcheadas por trucos de codigo.
#pragma once
#include <cstdint>
#include <map>

namespace nds {

struct Cpu;

// Todo lo que la CPU necesita del resto de la consola.
struct Bus {
    virtual ~Bus() = default;
    virtual uint32_t read32(uint32_t a) = 0;   // a alineada a 4
    virtual uint32_t read16(uint32_t a) = 0;   // a alineada a 2
    virtual uint32_t read8(uint32_t a) = 0;
    virtual void write32(uint32_t a, uint32_t v) = 0;
    virtual void write16(uint32_t a, uint32_t v) = 0;
    virtual void write8(uint32_t a, uint32_t v) = 0;
    // BIOS por HLE: devuelve true si atendio la SWI (si no, la CPU entra en la excepcion SVC).
    virtual bool swi(Cpu&, uint32_t /*numero*/) { return false; }
    // Coprocesador 15 (solo ARM9).
    virtual uint32_t cp15_read(uint32_t, uint32_t, uint32_t, uint32_t) { return 0; }
    virtual void cp15_write(uint32_t /*op1*/, uint32_t /*crn*/, uint32_t /*crm*/, uint32_t /*op2*/, uint32_t) {}
};

struct Cpu {
    enum Arch { ARM7_V4T, ARM9_V5TE };
    enum Mode : uint32_t { USR = 0x10, FIQ = 0x11, IRQ = 0x12, SVC = 0x13, ABT = 0x17, UND = 0x1B, SYS = 0x1F };

    Cpu(Arch a, Bus* b);
    void reset(uint32_t pc);                 // modo SVC, ARM, interrupciones desactivadas
    int step();                              // ejecuta UNA instruccion (o toma una IRQ). Devuelve ciclos aprox.

    // Estado (r[15] guarda la direccion de la instruccion actual + 8 en ARM / + 4 en Thumb)
    uint32_t r[16] = {};
    bool N = false, Z = false, C = false, V = false, Q = false;
    bool I = true, F = true, thumb = false;
    uint32_t mode = SVC;
    bool halted = false;                     // la pone Halt/IntrWait/CP15; la quita el sistema al llegar una IRQ
    bool irq_line = false;                   // lo pone el sistema cuando (IE & IF) != 0
    bool hle_intrwait_started = false;       // para IntrWait por HLE
    struct CallRec { uint32_t from, to; };
    CallRec calls[512] = {}; uint32_t ncalls = 0;           // registro circular de llamadas (BL/BLX) para depurar
    std::map<uint32_t, uint64_t>* call_hist = nullptr;     // si no es nulo, cuenta las llamadas por destino (depuracion)
    void log_call(uint32_t to) { calls[ncalls++ & 511] = {pc(), to}; if (call_hist) (*call_hist)[to]++; }
    uint64_t irq_entries = 0, swi_count = 0;             // diagnostico
    bool unaligned_seen = false;             // solo para tests: hubo un acceso desalineado

    uint32_t cpsr() const;
    void set_cpsr(uint32_t v, uint32_t field_mask = 0xF);   // MSR con mascara de campos c,x,s,f
    uint32_t pc() const { return r[15] - (thumb ? 4 : 8); } // direccion de la instruccion actual
    void set_state(const uint32_t regs[16], uint32_t cpsr_value);   // regs[15] = direccion real; para tests
    void jump(uint32_t target);              // salto sin cambio de estado ARM/Thumb
    void jump_bx(uint32_t target);           // salto con interworking (bit0 = Thumb)

    // Acceso rapido a memoria sin pasar por el bus virtual (el sistema las rellena; la primera que encaja gana).
    struct FastRegion { uint32_t lo = 0, hi = 0, mask = 0; uint8_t* p = nullptr; };
    FastRegion fast[3];

    Arch arch;
    Bus* bus;
    uint32_t vector_base = 0;                // 0xFFFF0000 en el ARM9, 0 en el ARM7

private:
    uint32_t bank_sp_[6] = {}, bank_lr_[6] = {}, spsr_[6] = {};
    uint32_t fiq_r_[5] = {}, usr_r_[5] = {};
    bool branched_ = false;

    inline uint8_t* fp_(uint32_t a) {
        for (int k = 0; k < 3; k++) { const FastRegion& f = fast[k]; if (a - f.lo < f.hi - f.lo) return f.p + ((a - f.lo) & f.mask); }
        return nullptr;
    }
    inline uint32_t rd32_(uint32_t a) { if (uint8_t* p = fp_(a)) { uint32_t v; __builtin_memcpy(&v, p, 4); return v; } return bus->read32(a); }
    inline uint32_t rd16_(uint32_t a) { if (uint8_t* p = fp_(a)) { uint16_t v; __builtin_memcpy(&v, p, 2); return v; } return bus->read16(a); }
    inline uint32_t rd8_(uint32_t a) { if (uint8_t* p = fp_(a)) return *p; return bus->read8(a); }
    inline void wr32_(uint32_t a, uint32_t v) { if (uint8_t* p = fp_(a)) { __builtin_memcpy(p, &v, 4); return; } bus->write32(a, v); }
    inline void wr16_(uint32_t a, uint32_t v) { if (uint8_t* p = fp_(a)) { uint16_t w = (uint16_t)v; __builtin_memcpy(p, &w, 2); return; } bus->write16(a, v); }
    inline void wr8_(uint32_t a, uint32_t v) { if (uint8_t* p = fp_(a)) { *p = (uint8_t)v; return; } bus->write8(a, v); }

    static int bank_of(uint32_t m);
    void switch_mode(uint32_t nm);
    uint32_t spsr_get() const;
    void spsr_set(uint32_t v);
    void restore_cpsr_from_spsr();
    void exception(uint32_t new_mode, uint32_t vec, uint32_t lr_value);

    bool cond(uint32_t c) const;
    uint32_t shift(unsigned type, uint32_t v, uint32_t amt, bool imm, bool& c) const;
    uint32_t alu(unsigned op, uint32_t a, uint32_t b, bool s, bool shc);
    static bool is_logical(unsigned op) { return op == 0 || op == 1 || op == 8 || op == 9 || op >= 12; }
    void set_nz(uint32_t v) { N = v >> 31; Z = v == 0; }

    uint32_t ld32(uint32_t a);
    uint32_t ld16(uint32_t a, bool sign);
    uint32_t ld8(uint32_t a, bool sign) { uint32_t v = rd8_(a) & 0xFF; return sign ? (uint32_t)(int32_t)(int8_t)v : v; }
    void st32(uint32_t a, uint32_t v) { if (a & 3) unaligned_seen = true; wr32_(a & ~3u, v); }
    void st16(uint32_t a, uint32_t v) { if (a & 1) unaligned_seen = true; wr16_(a & ~1u, v & 0xFFFF); }
    void st8(uint32_t a, uint32_t v) { wr8_(a, v & 0xFF); }

    void exec_arm(uint32_t i);
    void exec_thumb(uint32_t h);
    void arm_data_proc(uint32_t i);
    void arm_ldm_stm(uint32_t i);
    void arm_halfword(uint32_t i);
    void arm_mul(uint32_t i);
    void arm_dsp(uint32_t i);
    void undefined();
};

}  // namespace nds
