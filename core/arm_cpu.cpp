#include "arm_cpu.h"
#include <cstring>

namespace nds {

static inline uint32_t ror32(uint32_t v, unsigned s) { s &= 31; return s ? (v >> s) | (v << (32 - s)) : v; }

Cpu::Cpu(Arch a, Bus* b) : arch(a), bus(b) { vector_base = (a == ARM9_V5TE) ? 0xFFFF0000u : 0; reset(0); }

void Cpu::reset(uint32_t pc_) {
    std::memset(r, 0, sizeof r);
    std::memset(bank_sp_, 0, sizeof bank_sp_); std::memset(bank_lr_, 0, sizeof bank_lr_); std::memset(spsr_, 0, sizeof spsr_);
    N = Z = C = V = Q = false; I = F = true; thumb = false; mode = SVC; halted = false; irq_line = false;
    hle_intrwait_started = false;
    jump(pc_);
}

// ---------------- registros con banco ----------------
int Cpu::bank_of(uint32_t m) {
    switch (m) { case FIQ: return 1; case IRQ: return 2; case SVC: return 3; case ABT: return 4; case UND: return 5; default: return 0; }
}
void Cpu::switch_mode(uint32_t nm) {
    if (nm == mode) return;
    int ob = bank_of(mode), nb = bank_of(nm);
    if (ob != nb) {
        bank_sp_[ob] = r[13]; bank_lr_[ob] = r[14];
        if (ob == 1) { for (int k = 0; k < 5; k++) { fiq_r_[k] = r[8 + k]; r[8 + k] = usr_r_[k]; } }
        if (nb == 1) { for (int k = 0; k < 5; k++) { usr_r_[k] = r[8 + k]; r[8 + k] = fiq_r_[k]; } }
        r[13] = bank_sp_[nb]; r[14] = bank_lr_[nb];
    }
    mode = nm;
}
uint32_t Cpu::spsr_get() const { int b = bank_of(mode); return (mode == USR || mode == SYS) ? cpsr() : spsr_[b]; }
void Cpu::spsr_set(uint32_t v) { if (mode != USR && mode != SYS) spsr_[bank_of(mode)] = v; }

uint32_t Cpu::cpsr() const {
    return ((uint32_t)N << 31) | ((uint32_t)Z << 30) | ((uint32_t)C << 29) | ((uint32_t)V << 28) | ((uint32_t)Q << 27) |
           ((uint32_t)I << 7) | ((uint32_t)F << 6) | ((uint32_t)thumb << 5) | mode;
}
void Cpu::set_cpsr(uint32_t v, uint32_t mask) {
    if (mask & 8) { N = v >> 31; Z = (v >> 30) & 1; C = (v >> 29) & 1; V = (v >> 28) & 1; Q = (v >> 27) & 1; }
    if ((mask & 1) && mode != USR) {
        I = (v >> 7) & 1; F = (v >> 6) & 1;
        uint32_t nm = v & 0x1F;
        if (nm == USR || nm == FIQ || nm == IRQ || nm == SVC || nm == ABT || nm == UND || nm == SYS) switch_mode(nm);
    }
}
void Cpu::restore_cpsr_from_spsr() {
    uint32_t s = spsr_get();
    bool was_thumb = thumb;
    N = s >> 31; Z = (s >> 30) & 1; C = (s >> 29) & 1; V = (s >> 28) & 1; Q = (s >> 27) & 1;
    I = (s >> 7) & 1; F = (s >> 6) & 1; thumb = (s >> 5) & 1;
    uint32_t nm = s & 0x1F;
    if (nm == USR || nm == FIQ || nm == IRQ || nm == SVC || nm == ABT || nm == UND || nm == SYS) switch_mode(nm);
    (void)was_thumb;
}
void Cpu::set_state(const uint32_t regs[16], uint32_t c) {
    mode = SYS; for (int k = 0; k < 16; k++) r[k] = regs[k];
    N = c >> 31; Z = (c >> 30) & 1; C = (c >> 29) & 1; V = (c >> 28) & 1; Q = (c >> 27) & 1;
    I = (c >> 7) & 1; F = (c >> 6) & 1; thumb = (c >> 5) & 1;
    uint32_t nm = c & 0x1F; if (nm != SYS) switch_mode(nm);
    uint32_t a = regs[15];
    r[15] = thumb ? (a & ~1u) + 4 : (a & ~3u) + 8;
}
void Cpu::jump(uint32_t t) { r[15] = thumb ? (t & ~1u) + 4 : (t & ~3u) + 8; branched_ = true; }
void Cpu::jump_bx(uint32_t t) { thumb = t & 1; jump(t); }

void Cpu::exception(uint32_t nm, uint32_t vec, uint32_t lr_value) {
    uint32_t old = cpsr();
    switch_mode(nm);
    spsr_set(old);
    r[14] = lr_value;
    thumb = false; I = true;
    jump(vector_base + vec);
}
void Cpu::undefined() { exception(UND, 0x04, pc() + (thumb ? 2 : 4)); }

bool Cpu::cond(uint32_t c) const {
    switch (c) {
    case 0: return Z;  case 1: return !Z;  case 2: return C;  case 3: return !C;
    case 4: return N;  case 5: return !N;  case 6: return V;  case 7: return !V;
    case 8: return C && !Z;  case 9: return !C || Z;
    case 10: return N == V;  case 11: return N != V;
    case 12: return !Z && N == V;  case 13: return Z || N != V;
    default: return true;
    }
}

// ---------------- desplazamientos y ALU ----------------
uint32_t Cpu::shift(unsigned type, uint32_t v, uint32_t amt, bool imm, bool& c) const {
    if (imm) {
        switch (type) {
        case 0: if (amt == 0) return v; c = (v >> (32 - amt)) & 1; return v << amt;
        case 1: if (amt == 0) amt = 32; if (amt == 32) { c = v >> 31; return 0; } c = (v >> (amt - 1)) & 1; return v >> amt;
        case 2: if (amt == 0) amt = 32; if (amt >= 32) { c = v >> 31; return (uint32_t)((int32_t)v >> 31); } c = (v >> (amt - 1)) & 1; return (uint32_t)((int32_t)v >> amt);
        default:
            if (amt == 0) { bool oc = c; c = v & 1; return (v >> 1) | ((uint32_t)oc << 31); }   // RRX
            c = (v >> (amt - 1)) & 1; return ror32(v, amt);
        }
    }
    amt &= 0xFF;
    if (amt == 0) return v;
    switch (type) {
    case 0: if (amt < 32) { c = (v >> (32 - amt)) & 1; return v << amt; } c = (amt == 32) ? (v & 1) : 0; return 0;
    case 1: if (amt < 32) { c = (v >> (amt - 1)) & 1; return v >> amt; } c = (amt == 32) ? (v >> 31) : 0; return 0;
    case 2: if (amt < 32) { c = (v >> (amt - 1)) & 1; return (uint32_t)((int32_t)v >> amt); } c = v >> 31; return (uint32_t)((int32_t)v >> 31);
    default: { unsigned a = amt & 31; if (a == 0) { c = v >> 31; return v; } c = (v >> (a - 1)) & 1; return ror32(v, a); }
    }
}

uint32_t Cpu::alu(unsigned op, uint32_t a, uint32_t b, bool s, bool shc) {
    uint64_t t; uint32_t res;
    switch (op) {
    case 0: case 8:  res = a & b; break;
    case 1: case 9:  res = a ^ b; break;
    case 12: res = a | b; break;
    case 13: res = b; break;
    case 14: res = a & ~b; break;
    case 15: res = ~b; break;
    case 2: case 10: t = (uint64_t)a + (uint32_t)~b + 1; res = (uint32_t)t;
        if (s) { C = t >> 32; V = ((a ^ b) & (a ^ res)) >> 31; } break;
    case 3: t = (uint64_t)b + (uint32_t)~a + 1; res = (uint32_t)t;
        if (s) { C = t >> 32; V = ((a ^ b) & (b ^ res)) >> 31; } break;
    case 4: case 11: t = (uint64_t)a + b; res = (uint32_t)t;
        if (s) { C = t >> 32; V = (~(a ^ b) & (a ^ res)) >> 31; } break;
    case 5: t = (uint64_t)a + b + C; res = (uint32_t)t;
        if (s) { C = t >> 32; V = (~(a ^ b) & (a ^ res)) >> 31; } break;
    case 6: t = (uint64_t)a + (uint32_t)~b + C; res = (uint32_t)t;
        if (s) { C = t >> 32; V = ((a ^ b) & (a ^ res)) >> 31; } break;
    default: t = (uint64_t)b + (uint32_t)~a + C; res = (uint32_t)t;
        if (s) { C = t >> 32; V = ((a ^ b) & (b ^ res)) >> 31; } break;
    }
    if (s) { set_nz(res); if (is_logical(op)) C = shc; }
    return res;
}

// ---------------- acceso a memoria con las rarezas de alineacion ----------------
uint32_t Cpu::ld32(uint32_t a) { if (a & 3) unaligned_seen = true; return ror32(rd32_(a & ~3u), (a & 3) * 8); }
uint32_t Cpu::ld16(uint32_t a, bool sign) {
    if (a & 1) unaligned_seen = true;
    uint32_t v = rd16_(a & ~1u) & 0xFFFF;
    if (arch == ARM7_V4T && (a & 1)) {                       // ARM7: LDRH rota; LDRSH extiende el byte
        return sign ? (uint32_t)(int32_t)(int8_t)(v >> 8) : ror32(v, 8);
    }
    return sign ? (uint32_t)(int32_t)(int16_t)v : v;
}

// ---------------- paso principal ----------------
int Cpu::step() {
    if (halted) return 1;
    if (irq_line && !I) {                                    // IRQ: LR = siguiente instruccion + 4
        irq_entries++; exception(IRQ, 0x18, pc() + 4);
        return 3;
    }
    branched_ = false;
    if (thumb) {
        uint32_t h = rd16_(r[15] - 4) & 0xFFFF;
        exec_thumb(h);
        if (!branched_) r[15] += 2;
    } else {
        uint32_t i = rd32_(r[15] - 8);
        exec_arm(i);
        if (!branched_) r[15] += 4;
    }
    return 1;
}

// ---------------- ARM ----------------
void Cpu::exec_arm(uint32_t i) {
    uint32_t c = i >> 28;
    const bool v5 = (arch == ARM9_V5TE);
    if (c == 0xF) {
        if (!v5) return;                                     // ARM7: NV nunca se ejecuta
        if (((i >> 25) & 7) == 5) {                          // BLX inmediato
            uint32_t off = (uint32_t)((int32_t)(i << 8) >> 6) + ((i >> 23) & 2);
            uint32_t target = r[15] + off;
            r[14] = pc() + 4; log_call(target);
            thumb = true; jump(target | 1);
            return;
        }
        if ((i & 0xFD70F000) == 0xF550F000) return;         // PLD
        undefined(); return;
    }
    if (!cond(c)) return;

    switch ((i >> 25) & 7) {
    case 0: case 1: {
        if (((i >> 25) & 7) == 0 && (i & 0x90) == 0x90) {
            if ((i & 0x60) == 0) arm_mul(i); else arm_halfword(i);
            return;
        }
        if ((i & 0x01900000) == 0x01000000) {                // zona "miscelanea" (opcodes TST..CMN sin S)
            if (((i >> 25) & 1) == 0) {
                if ((i & 0x0FFFFFF0) == 0x012FFF10) { jump_bx(r[i & 15]); return; }       // BX
                if (v5 && (i & 0x0FFFFFF0) == 0x012FFF30) {                                 // BLX reg
                    uint32_t t = r[i & 15]; r[14] = pc() + 4; log_call(t); jump_bx(t); return; }
                if (v5 && (i & 0x0FFF0FF0) == 0x016F0F10) {                                 // CLZ
                    uint32_t v = r[i & 15]; uint32_t n = 0; if (v == 0) n = 32; else while (!(v & 0x80000000)) { v <<= 1; n++; }
                    r[(i >> 12) & 15] = n; return; }
                if (v5 && (i & 0x0F900FF0) == 0x01000050) { arm_dsp(i); return; }           // QADD..QDSUB
                if (v5 && (i & 0x0F900090) == 0x01000080) { arm_dsp(i); return; }           // SMUL/SMLA
                if ((i & 0x0FBF0FFF) == 0x010F0000) {                                       // MRS
                    r[(i >> 12) & 15] = (i & 0x400000) ? spsr_get() : cpsr(); return; }
                if ((i & 0x0FB0FFF0) == 0x0120F000) {                                       // MSR reg
                    uint32_t v = r[i & 15], m = (i >> 16) & 0xF;
                    if (i & 0x400000) { if (mode != USR && mode != SYS) { uint32_t o = spsr_get(), nv = o;
                        if (m & 1) nv = (nv & ~0xFFu) | (v & 0xFF); if (m & 8) nv = (nv & 0x00FFFFFF) | (v & 0xFF000000); spsr_set(nv); } }
                    else set_cpsr(v, m & 9);
                    return; }
                undefined(); return;
            } else if ((i & 0x0FB0F000) == 0x0320F000) {                                    // MSR imm
                uint32_t v = ror32(i & 0xFF, ((i >> 8) & 15) * 2), m = (i >> 16) & 0xF;
                if (i & 0x400000) { if (mode != USR && mode != SYS) { uint32_t nv = spsr_get();
                    if (m & 1) nv = (nv & ~0xFFu) | (v & 0xFF); if (m & 8) nv = (nv & 0x00FFFFFF) | (v & 0xFF000000); spsr_set(nv); } }
                else set_cpsr(v, m & 9);
                return;
            } else if ((i & 0x0FFFFF00) == 0x0320F000) return;                              // NOP / hints
            undefined(); return;
        }
        arm_data_proc(i); return;
    }
    case 2: case 3: {                                        // LDR/STR
        if (((i >> 25) & 1) && (i & 0x10)) { undefined(); return; }
        bool P = i & (1 << 24), U = i & (1 << 23), B = i & (1 << 22), W = i & (1 << 21), L = i & (1 << 20);
        uint32_t rn = (i >> 16) & 15, rd = (i >> 12) & 15, off;
        if (!((i >> 25) & 1)) off = i & 0xFFF;
        else { bool cc = C; off = shift((i >> 5) & 3, r[i & 15], (i >> 7) & 31, true, cc); }
        uint32_t base = r[rn], addr = P ? (U ? base + off : base - off) : base;
        uint32_t nb = U ? base + off : base - off;
        if (L) {
            uint32_t v = B ? ld8(addr, false) : ld32(addr);
            if ((P && W) || !P) r[rn] = nb;                   // writeback antes de cargar Rd: Rd gana si Rd==Rn
            if (rd == 15) { if (v5 && !B) jump_bx(v); else jump(v); } else r[rd] = v;
        } else {
            uint32_t v = r[rd]; if (rd == 15) v += 4;
            if (B) st8(addr, v); else st32(addr, v);
            if ((P && W) || !P) r[rn] = nb;
        }
        return;
    }
    case 4: arm_ldm_stm(i); return;
    case 5: {                                                // B / BL
        uint32_t off = (uint32_t)((int32_t)(i << 8) >> 6);
        if (i & (1 << 24)) { r[14] = pc() + 4; log_call(r[15] + off); }
        jump(r[15] + off);
        return;
    }
    case 7:
        if (i & (1 << 24)) {                                 // SWI
            swi_count++; if (bus->swi(*this, i & 0xFFFFFF)) return;
            exception(SVC, 0x08, pc() + 4); return;
        }
        if ((i & 0x10) && ((i >> 8) & 15) == 15 && v5) {     // MRC/MCR p15
            uint32_t op1 = (i >> 21) & 7, crn = (i >> 16) & 15, rd = (i >> 12) & 15, op2 = (i >> 5) & 7, crm = i & 15;
            if (i & (1 << 20)) { uint32_t v = bus->cp15_read(op1, crn, crm, op2); if (rd == 15) { N = v >> 31; Z = (v >> 30) & 1; C = (v >> 29) & 1; V = (v >> 28) & 1; } else r[rd] = v; }
            else bus->cp15_write(op1, crn, crm, op2, r[rd]);
            return;
        }
        undefined(); return;
    default: undefined(); return;                            // coprocesador LDC/STC/CDP
    }
}

void Cpu::arm_data_proc(uint32_t i) {
    unsigned op = (i >> 21) & 15; bool s = i & (1 << 20);
    uint32_t rn = (i >> 16) & 15, rd = (i >> 12) & 15;
    bool shc = C; uint32_t b, a = r[rn];
    if ((i >> 25) & 1) {
        unsigned rot = ((i >> 8) & 15) * 2; b = ror32(i & 0xFF, rot); if (rot) shc = b >> 31;
    } else if (i & 0x10) {                                   // desplazamiento por registro: PC se lee como +12
        uint32_t rm = r[i & 15]; if ((i & 15) == 15) rm += 4; if (rn == 15) a += 4;
        b = shift((i >> 5) & 3, rm, r[(i >> 8) & 15], false, shc);
    } else {
        b = shift((i >> 5) & 3, r[i & 15], (i >> 7) & 31, true, shc);
    }
    bool test = (op >= 8 && op <= 11);
    bool wr_pc = (rd == 15 && !test);
    uint32_t res = alu(op, a, b, s && !(wr_pc), shc);
    if (test) return;
    if (wr_pc) {
        if (s) { restore_cpsr_from_spsr(); }
        jump(res);
    } else r[rd] = res;
}

void Cpu::arm_mul(uint32_t i) {
    bool s = i & (1 << 20);
    uint32_t rm = r[i & 15], rs = r[(i >> 8) & 15];
    if ((i & 0x0FC000F0) == 0x00000090) {                    // MUL / MLA
        uint32_t rd = (i >> 16) & 15, rn = (i >> 12) & 15;
        uint32_t v = rm * rs; if (i & (1 << 21)) v += r[rn];
        r[rd] = v; if (s) set_nz(v);
    } else if ((i & 0x0F8000F0) == 0x00800090) {             // UMULL/UMLAL/SMULL/SMLAL
        uint32_t hi = (i >> 16) & 15, lo = (i >> 12) & 15;
        uint64_t p;
        if (i & (1 << 22)) p = (uint64_t)((int64_t)(int32_t)rm * (int64_t)(int32_t)rs); else p = (uint64_t)rm * rs;
        if (i & (1 << 21)) p += ((uint64_t)r[hi] << 32) | r[lo];
        r[lo] = (uint32_t)p; r[hi] = (uint32_t)(p >> 32);
        if (s) { N = r[hi] >> 31; Z = (r[hi] | r[lo]) == 0; }
    } else if ((i & 0x0FB00FF0) == 0x01000090) {             // SWP / SWPB
        uint32_t rn = (i >> 16) & 15, rd = (i >> 12) & 15; uint32_t a = r[rn];
        if (i & (1 << 22)) { uint32_t o = ld8(a, false); st8(a, rm); r[rd] = o; }
        else { uint32_t o = ld32(a); st32(a, rm); r[rd] = o; }
    } else undefined();
}

void Cpu::arm_halfword(uint32_t i) {
    bool P = i & (1 << 24), U = i & (1 << 23), Imm = i & (1 << 22), W = i & (1 << 21), L = i & (1 << 20);
    uint32_t rn = (i >> 16) & 15, rd = (i >> 12) & 15, sh = (i >> 5) & 3;
    uint32_t off = Imm ? (((i >> 4) & 0xF0) | (i & 0xF)) : r[i & 15];
    uint32_t base = r[rn], nb = U ? base + off : base - off, addr = P ? nb : base;
    bool wb = (P && W) || !P;
    if (!L && (sh == 2 || sh == 3)) {                        // LDRD / STRD (solo ARMv5TE)
        if (addr & 3) unaligned_seen = true;
        if (arch != ARM9_V5TE || (rd & 1) || rd == 14) { undefined(); return; }
        if (sh == 2) { r[rd] = rd32_(addr & ~3u); r[rd + 1] = rd32_((addr + 4) & ~3u); if (wb) r[rn] = nb; if (rd + 1 == 15) {} }
        else { uint32_t v0 = r[rd], v1 = r[rd + 1]; wr32_(addr & ~3u, v0); wr32_((addr + 4) & ~3u, v1); if (wb) r[rn] = nb; }
        return;
    }
    if (L) {
        uint32_t v = (sh == 1) ? ld16(addr, false) : (sh == 2) ? ld8(addr, true) : ld16(addr, true);
        if (wb) r[rn] = nb;
        if (rd == 15) jump(v); else r[rd] = v;
    } else if (sh == 1) {
        uint32_t v = r[rd]; if (rd == 15) v += 4;
        st16(addr, v); if (wb) r[rn] = nb;
    } else undefined();
}

void Cpu::arm_dsp(uint32_t i) {
    auto sat_add = [&](int32_t a, int32_t b) -> int32_t { int64_t t = (int64_t)a + b; if (t > INT32_MAX) { Q = true; return INT32_MAX; } if (t < INT32_MIN) { Q = true; return INT32_MIN; } return (int32_t)t; };
    auto sat_sub = [&](int32_t a, int32_t b) -> int32_t { int64_t t = (int64_t)a - b; if (t > INT32_MAX) { Q = true; return INT32_MAX; } if (t < INT32_MIN) { Q = true; return INT32_MIN; } return (int32_t)t; };
    if ((i & 0x0F900FF0) == 0x01000050) {                    // QADD, QSUB, QDADD, QDSUB
        uint32_t rd = (i >> 12) & 15; int32_t rm = (int32_t)r[i & 15], rn = (int32_t)r[(i >> 16) & 15];
        unsigned op = (i >> 21) & 3;
        if (op & 2) rn = sat_add(rn, rn);
        r[rd] = (uint32_t)((op & 1) ? sat_sub(rm, rn) : sat_add(rm, rn));
        return;
    }
    unsigned op = (i >> 21) & 3; bool x = i & 0x20, y = i & 0x40;
    uint32_t rd = (i >> 16) & 15, rn = (i >> 12) & 15; int32_t rm = (int32_t)r[i & 15], rs = (int32_t)r[(i >> 8) & 15];
    auto half = [](int32_t v, bool top) -> int32_t { return top ? (int32_t)(v >> 16) : (int32_t)(int16_t)v; };
    auto acc_q = [&](int32_t prod, int32_t acc) -> int32_t { int64_t t = (int64_t)prod + acc; if (t > INT32_MAX || t < INT32_MIN) Q = true; return (int32_t)t; };
    switch (op) {
    case 0: r[rd] = (uint32_t)acc_q(half(rm, x) * half(rs, y), (int32_t)r[rn]); break;           // SMLAxy
    case 1: { int64_t p = ((int64_t)rm * half(rs, y)) >> 16;
        if (x) r[rd] = (uint32_t)(int32_t)p;                                                       // SMULWy
        else r[rd] = (uint32_t)acc_q((int32_t)p, (int32_t)r[rn]); break; }                         // SMLAWy
    case 2: { int64_t p = (int64_t)half(rm, x) * half(rs, y);                                      // SMLALxy
        uint64_t acc = ((uint64_t)r[rd] << 32) | r[rn]; acc += (uint64_t)p;
        r[rn] = (uint32_t)acc; r[rd] = (uint32_t)(acc >> 32); break; }
    default: r[rd] = (uint32_t)(half(rm, x) * half(rs, y)); break;                                 // SMULxy
    }
}

void Cpu::arm_ldm_stm(uint32_t i) {
    bool P = i & (1 << 24), U = i & (1 << 23), S = i & (1 << 22), W = i & (1 << 21), L = i & (1 << 20);
    uint32_t rn = (i >> 16) & 15, list = i & 0xFFFF, base = r[rn];
    unsigned n = __builtin_popcount(list);
    uint32_t start, nb;
    if (n == 0) { if (arch == ARM9_V5TE) { if (W) r[rn] = U ? base + 0x40 : base - 0x40; return; } list = 1u << 15; n = 16; }
    if (U) { start = P ? base + 4 : base; nb = base + 4 * n; } else { start = P ? base - 4 * n : base - 4 * n + 4; nb = base - 4 * n; }
    bool user_bank = S && !(L && (list & 0x8000));
    uint32_t saved_mode = mode;
    if (user_bank && mode != USR && mode != SYS) switch_mode(SYS);   // acceso al banco de usuario
    if (start & 3) unaligned_seen = true;
    uint32_t a = start;
    bool first = true;
    for (int k = 0; k < 16; k++) {
        if (!(list & (1u << k))) continue;
        if (L) { uint32_t v = rd32_(a & ~3u);
            if (k == 15) { if (S) restore_cpsr_from_spsr(); if (arch == ARM9_V5TE && !S) jump_bx(v); else jump(v); } else r[k] = v;
        } else { uint32_t v = r[k]; if (k == 15) v += 4;
            if (k == (int)rn && W && !first && arch == ARM7_V4T) v = nb;   // ARM7 guarda el valor ya actualizado si no es el primero
            wr32_(a & ~3u, v); }
        a += 4; first = false;
    }
    if (user_bank && saved_mode != USR && saved_mode != SYS) switch_mode(saved_mode);
    if (W) { if (!(L && (list & (1u << rn)))) r[rn] = nb; }
}

// ---------------- Thumb ----------------
void Cpu::exec_thumb(uint32_t h) {
    const bool v5 = (arch == ARM9_V5TE);
    switch (h >> 11) {
    case 0: case 1: case 2: {                                // LSL/LSR/ASR inmediato
        bool c = C; uint32_t rs = r[(h >> 3) & 7];
        uint32_t v = shift(h >> 11, rs, (h >> 6) & 31, true, c);
        C = c; r[h & 7] = v; set_nz(v); return; }
    case 3: {                                                // ADD/SUB
        uint32_t a = r[(h >> 3) & 7], b = (h & (1 << 10)) ? ((h >> 6) & 7) : r[(h >> 6) & 7];
        r[h & 7] = alu((h & (1 << 9)) ? 2 : 4, a, b, true, false); return; }
    case 4: r[(h >> 8) & 7] = alu(13, 0, h & 0xFF, true, C); return;                       // MOV
    case 5: alu(10, r[(h >> 8) & 7], h & 0xFF, true, false); return;                        // CMP
    case 6: r[(h >> 8) & 7] = alu(4, r[(h >> 8) & 7], h & 0xFF, true, false); return;      // ADD
    case 7: r[(h >> 8) & 7] = alu(2, r[(h >> 8) & 7], h & 0xFF, true, false); return;      // SUB
    case 8: {
        if (!(h & (1 << 10))) {                              // operaciones ALU
            unsigned op = (h >> 6) & 15; uint32_t rd = h & 7, a = r[rd], b = r[(h >> 3) & 7]; bool c = C;
            switch (op) {
            case 0: r[rd] = alu(0, a, b, true, C); break;     case 1: r[rd] = alu(1, a, b, true, C); break;
            case 2: { uint32_t v = shift(0, a, b, false, c); C = c; r[rd] = v; set_nz(v); break; }
            case 3: { uint32_t v = shift(1, a, b, false, c); C = c; r[rd] = v; set_nz(v); break; }
            case 4: { uint32_t v = shift(2, a, b, false, c); C = c; r[rd] = v; set_nz(v); break; }
            case 5: r[rd] = alu(5, a, b, true, false); break;  case 6: r[rd] = alu(6, a, b, true, false); break;
            case 7: { uint32_t v = shift(3, a, b, false, c); C = c; r[rd] = v; set_nz(v); break; }
            case 8: alu(8, a, b, true, C); break;
            case 9: r[rd] = alu(3, b, 0, true, false); break;  // NEG = 0 - Rs
            case 10: alu(10, a, b, true, false); break;       case 11: alu(11, a, b, true, false); break;
            case 12: r[rd] = alu(12, a, b, true, C); break;
            case 13: { uint32_t v = a * b; r[rd] = v; set_nz(v); break; }
            case 14: r[rd] = alu(14, a, b, true, C); break;   default: r[rd] = alu(15, a, b, true, C); break;
            }
            return;
        }
        unsigned op = (h >> 8) & 3; uint32_t rd = (h & 7) | ((h >> 4) & 8), rs = (h >> 3) & 15;
        uint32_t b = r[rs];
        switch (op) {
        case 0: { uint32_t v = r[rd] + b; if (rd == 15) jump(v); else r[rd] = v; return; }
        case 1: alu(10, r[rd], b, true, false); return;
        case 2: if (rd == 15) jump(b); else r[rd] = b; return;
        default:
            if (h & 0x80) { if (!v5) { undefined(); return; } r[14] = (pc() + 2) | 1; log_call(b); }     // BLX reg
            jump_bx(b); return;
        } }
    case 9: { uint32_t a = (r[15] & ~3u) + ((h & 0xFF) << 2); r[(h >> 8) & 7] = rd32_(a); return; }   // LDR PC-rel
    case 10: case 11: {                                      // load/store con registro
        uint32_t rd = h & 7, a = r[(h >> 3) & 7] + r[(h >> 6) & 7]; unsigned op = (h >> 9) & 7;
        switch (op) {
        case 0: st32(a, r[rd]); break;     case 1: st16(a, r[rd]); break;      case 2: st8(a, r[rd]); break;
        case 3: r[rd] = ld8(a, true); break;   case 4: r[rd] = ld32(a); break;
        case 5: r[rd] = ld16(a, false); break; case 6: r[rd] = ld8(a, false); break; default: r[rd] = ld16(a, true); break;
        }
        return; }
    case 12: { uint32_t a = r[(h >> 3) & 7] + ((h >> 4) & 0x7C); st32(a, r[h & 7]); return; }
    case 13: { uint32_t a = r[(h >> 3) & 7] + ((h >> 4) & 0x7C); r[h & 7] = ld32(a); return; }
    case 14: { uint32_t a = r[(h >> 3) & 7] + ((h >> 6) & 31); st8(a, r[h & 7]); return; }
    case 15: { uint32_t a = r[(h >> 3) & 7] + ((h >> 6) & 31); r[h & 7] = ld8(a, false); return; }
    case 16: { uint32_t a = r[(h >> 3) & 7] + ((h >> 5) & 0x3E); st16(a, r[h & 7]); return; }
    case 17: { uint32_t a = r[(h >> 3) & 7] + ((h >> 5) & 0x3E); r[h & 7] = ld16(a, false); return; }
    case 18: { uint32_t a = r[13] + ((h & 0xFF) << 2); st32(a, r[(h >> 8) & 7]); return; }
    case 19: { uint32_t a = r[13] + ((h & 0xFF) << 2); r[(h >> 8) & 7] = ld32(a); return; }
    case 20: r[(h >> 8) & 7] = (r[15] & ~3u) + ((h & 0xFF) << 2); return;     // ADD Rd, PC, #imm
    case 21: r[(h >> 8) & 7] = r[13] + ((h & 0xFF) << 2); return;              // ADD Rd, SP, #imm
    case 22: case 23: {
        if ((h & 0xFF00) == 0xB000) { uint32_t off = (h & 0x7F) << 2; r[13] = (h & 0x80) ? r[13] - off : r[13] + off; return; }
        if ((h & 0xF600) == 0xB400) {                        // PUSH / POP
            uint32_t list = h & 0xFF; bool extra = h & 0x100; unsigned n = __builtin_popcount(list) + extra;
            if (r[13] & 3) unaligned_seen = true;
            if (!(h & 0x800)) {                              // PUSH
                uint32_t a = r[13] - 4 * n; r[13] = a;
                for (int k = 0; k < 8; k++) if (list & (1u << k)) { wr32_(a & ~3u, r[k]); a += 4; }
                if (extra) wr32_(a & ~3u, r[14]);
            } else {                                         // POP
                uint32_t a = r[13];
                for (int k = 0; k < 8; k++) if (list & (1u << k)) { r[k] = rd32_(a & ~3u); a += 4; }
                if (extra) { uint32_t v = rd32_(a & ~3u); a += 4; r[13] = a; if (v5) jump_bx(v); else jump(v); return; }
                r[13] = a;
            }
            return;
        }
        if (v5 && (h & 0xFF00) == 0xBE00) { undefined(); return; }   // BKPT
        undefined(); return; }
    case 24: case 25: {                                      // STMIA / LDMIA
        uint32_t rn = (h >> 8) & 7, list = h & 0xFF, a = r[rn]; unsigned n = __builtin_popcount(list);
        bool L = h & 0x800; if (a & 3) unaligned_seen = true;
        if (n == 0) { if (v5) { r[rn] += 0x40; } return; }
        for (int k = 0; k < 8; k++) if (list & (1u << k)) { if (L) r[k] = rd32_(a & ~3u); else wr32_(a & ~3u, r[k]); a += 4; }
        if (!(L && (list & (1u << rn)))) r[rn] = a;
        return; }
    case 26: case 27: {
        if ((h & 0xFF00) == 0xDF00) {                        // SWI
            if (bus->swi(*this, h & 0xFF)) return;
            exception(SVC, 0x08, pc() + 2); return;
        }
        if ((h & 0x0F00) == 0x0E00) { undefined(); return; }
        if (!cond((h >> 8) & 15)) return;
        jump(r[15] + (uint32_t)((int32_t)(int8_t)(h & 0xFF) << 1)); return; }
    case 28: jump(r[15] + (uint32_t)(((int32_t)(h << 21)) >> 20)); return;      // B
    case 29:                                                 // BLX sufijo (solo v5)
        if (!v5 || (h & 1)) { undefined(); return; }
        { uint32_t t = r[14] + ((h & 0x7FF) << 1); r[14] = (pc() + 2) | 1; log_call(t & ~3u); thumb = false; jump(t & ~3u); }
        return;
    case 30: r[14] = r[15] + (uint32_t)(((int32_t)(h << 21)) >> 9); return;     // BL prefijo
    default: { uint32_t t = r[14] + ((h & 0x7FF) << 1); r[14] = (pc() + 2) | 1; log_call(t); jump(t); return; }   // BL sufijo
    }
}

}  // namespace nds
