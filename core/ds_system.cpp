#include "ds_system.h"
#include "nds_rom.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace nds {

// ---------- Bus ----------
uint32_t ConsoleBus::read32(uint32_t a) { return c->mem_read(id, a, 4); }
uint32_t ConsoleBus::read16(uint32_t a) { return c->mem_read(id, a, 2); }
uint32_t ConsoleBus::read8(uint32_t a)  { return c->mem_read(id, a, 1); }
void ConsoleBus::write32(uint32_t a, uint32_t v) { c->mem_write(id, a, v, 4); }
void ConsoleBus::write16(uint32_t a, uint32_t v) { c->mem_write(id, a, v, 2); }
void ConsoleBus::write8(uint32_t a, uint32_t v)  { c->mem_write(id, a, v, 1); }
bool ConsoleBus::swi(Cpu& cpu, uint32_t n) { return c->hle_swi(id, cpu, n); }

uint32_t ConsoleBus::cp15_read(uint32_t op1, uint32_t crn, uint32_t crm, uint32_t op2) {
    if (op1 != 0) return 0;
    if (crn == 0 && crm == 0) return op2 == 1 ? 0x0F0D2112 : 0x41059461;      // ID y tipo de cache del ARM946E-S
    if (crn == 1 && crm == 0 && op2 == 0) return c->cp15_ctrl;
    if (crn == 9 && crm == 1) return op2 == 0 ? c->dtcm_reg : c->itcm_reg;
    return 0;
}
void ConsoleBus::cp15_write(uint32_t op1, uint32_t crn, uint32_t crm, uint32_t op2, uint32_t v) {
    if (op1 != 0) return;
    if (crn == 1 && crm == 0 && op2 == 0) { c->cp15_ctrl = v; c->dtcm_on = v & (1u << 16); c->itcm_on = v & (1u << 18); c->update_fast(); return; }
    if (crn == 9 && crm == 1) {
        if (op2 == 0) { c->dtcm_reg = v; c->dtcm_base = v & 0xFFFFF000u; c->dtcm_size = 512u << ((v >> 1) & 0x1F); }
        else { c->itcm_reg = v; c->itcm_size = 512u << ((v >> 1) & 0x1F); }
        c->update_fast();
        return;
    }
    if (crn == 7 && crm == 0 && op2 == 4) { c->cpu9.halted = true; return; }    // esperar interrupcion
    // caches, buffers de escritura, proteccion de memoria: sin efecto en esta emulacion
}

// Regiones de acceso rapido de cada CPU (hay que recalcularlas cuando cambia la configuracion de la TCM)
void Console::update_fast() {
    cpu9.fast[0] = dtcm_on ? Cpu::FastRegion{dtcm_base, dtcm_base + dtcm_size, 0x3FFF, dtcm} : Cpu::FastRegion{};
    cpu9.fast[1] = itcm_on ? Cpu::FastRegion{0, itcm_size, 0x7FFF, itcm} : Cpu::FastRegion{};
    cpu9.fast[2] = Cpu::FastRegion{0x02000000, 0x03000000, 0x3FFFFF, main_ram};
    cpu7.fast[0] = Cpu::FastRegion{0x02000000, 0x03000000, 0x3FFFFF, main_ram};
    cpu7.fast[1] = Cpu::FastRegion{0x03800000, 0x04000000, 0xFFFF, wram7};
    cpu7.fast[2] = Cpu::FastRegion{};
}

// ---------- memoria ----------
uint8_t* Console::ptr(int cpu, uint32_t a, bool write) {
    uint32_t region = a >> 24;
    if (cpu == 0) {
        if (a >= 0xFFFF0000u) return write ? nullptr : bios9 + (a & 0xFFF);
        if (dtcm_on && a >= dtcm_base && a - dtcm_base < dtcm_size) return dtcm + ((a - dtcm_base) & 0x3FFF);
        if (itcm_on && a < itcm_size) return itcm + (a & 0x7FFF);
        switch (region) {
        case 2: return main_ram + (a & 0x3FFFFF);
        case 3: switch (wramcnt) { case 0: return shared_wram + (a & 0x7FFF); case 1: return shared_wram + 0x4000 + (a & 0x3FFF);
                                   case 2: return shared_wram + (a & 0x3FFF); default: return nullptr; }
        case 5: return pal + (a & 0x7FF);
        case 6: {
            uint8_t* pg = nullptr; uint32_t off = a & 0x3FFF;
            if (a >= 0x06800000) { uint32_t o = a - 0x06800000; if (o < 0xA4000) pg = map_lcdc_[o >> 14]; }
            else switch ((a >> 21) & 3) { case 0: pg = map_bga_[(a & 0x7FFFF) >> 14]; break; case 1: pg = map_bgb_[(a & 0x1FFFF) >> 14]; break;
                                          case 2: pg = map_obja_[(a & 0x3FFFF) >> 14]; break; default: pg = map_objb_[(a & 0x1FFFF) >> 14]; break; }
            return pg ? pg + off : nullptr; }
        case 7: return oam + (a & 0x7FF);
        default: return nullptr;
        }
    }
    if (a < 0x4000) return write ? nullptr : bios7 + (a & 0xFFF);
    switch (region) {
    case 2: return main_ram + (a & 0x3FFFFF);
    case 3:
        if (a >= 0x03800000) return wram7 + (a & 0xFFFF);
        switch (wramcnt) { case 0: return wram7 + (a & 0xFFFF); case 1: return shared_wram + (a & 0x3FFF);
                           case 2: return shared_wram + 0x4000 + (a & 0x3FFF); default: return shared_wram + (a & 0x7FFF); }
    default: return nullptr;
    }
}

uint32_t Console::mem_read(int cpu, uint32_t a, int size) {
    if (rd_log[cpu]) (*rd_log[cpu])[a & ~3u]++;
    if (uint8_t* p = ptr(cpu, a, false)) { uint32_t v = 0; std::memcpy(&v, p, size); return v; }
    uint32_t region = a >> 24;
    if (region == 4) {
        if (a < 0x04100000) {
            if (size == 4) return io_r32(cpu, a);
            if (size == 2) return io_r16(cpu, a);
            return (io_r16(cpu, a & ~1u) >> (8 * (a & 1))) & 0xFF;
        }
        if (a == 0x04100000) return ipc_recv(cpu);
        if (a == 0x04100010) {
            uint32_t v = 0;
            if (!card_words.empty()) { v = card_words.front(); card_words.pop_front();
                if (card_words.empty()) { romctrl &= ~((1u << 31) | (1u << 23)); if (auxspicnt & 0x4000) raise_irq(card_owner, 19); } }
            return v;
        }
        return 0;
    }
    if (region >= 8 && region < 0x0A) return size == 4 ? 0xFFFFFFFFu : size == 2 ? 0xFFFFu : 0xFFu;   // ranura GBA vacia
    return 0;
}

void Console::mem_write(int cpu, uint32_t a, uint32_t v, int size) {
    if (uint8_t* p = ptr(cpu, a, true)) { std::memcpy(p, &v, size); return; }
    if ((a >> 24) == 4 && a < 0x04100000) {
        if (size == 4) io_w32(cpu, a, v);
        else if (size == 2) io_w16(cpu, a, (uint16_t)v);
        else io_w8(cpu, a, (uint8_t)v);
    }
}

// ---------- E/S ----------
void Console::update_irq_lines() {
    Cpu* cpus[2] = {&cpu9, &cpu7};
    for (int i = 0; i < 2; i++) {
        bool pending = (IE[i] & IF[i]) != 0;
        cpus[i]->irq_line = (IME[i] & 1) && pending;
        if (pending) cpus[i]->halted = false;
    }
}

uint16_t Console::ipc_cnt(int c) {
    int o = c ^ 1; uint16_t v = fifocnt[c] & 0x8404;             // bits 2, 10, 15 guardados
    if (fifo[c].empty()) v |= 1; if (fifo[c].size() >= 16) v |= 2;
    if (fifo[o].empty()) v |= 0x100; if (fifo[o].size() >= 16) v |= 0x200;
    v |= fifocnt[c] & 0x4000;
    return v;
}
void Console::ipc_cnt_write(int c, uint16_t v) {
    uint16_t old = fifocnt[c];
    fifocnt[c] = (old & 0x4000) | (v & 0x8404);
    if (v & 0x4000) fifocnt[c] &= ~0x4000;                        // el error se borra escribiendo 1
    if (v & 0x8) fifo[c].clear();
    if ((v & 4) && !(old & 4) && fifo[c].empty()) raise_irq(c, 17);
    if ((v & 0x400) && !(old & 0x400) && !fifo[c ^ 1].empty()) raise_irq(c, 18);
}
void Console::ipc_send(int c, uint32_t v) {
    if (!(fifocnt[c] & 0x8000)) return;
    if (fifo[c].size() >= 16) { fifocnt[c] |= 0x4000; return; }
    bool was_empty = fifo[c].empty(); fifo[c].push_back(v); fifo_pushed[c]++; if (fifo_log && fifo_log->size() < 4000) fifo_log->push_back({c, v});
    if (was_empty && (fifocnt[c ^ 1] & 0x400)) raise_irq(c ^ 1, 18);
}
uint32_t Console::ipc_recv(int c) {
    int o = c ^ 1;
    if (fifo[o].empty()) { fifocnt[c] |= 0x4000; return fifo_last[c]; }
    uint32_t v = fifo[o].front(); fifo[o].pop_front(); fifo_last[c] = v; fifo_popped[c]++;
    if (fifo[o].empty() && (fifocnt[o] & 4)) raise_irq(o, 17);
    return v;
}

void Console::div_update() {
    int mode = divcnt & 3; int64_t n, d; bool zero;
    if (mode == 0) { n = (int32_t)div_numer; d = (int32_t)div_denom; } else if (mode == 1) { n = (int64_t)div_numer; d = (int32_t)div_denom; }
    else { n = (int64_t)div_numer; d = (int64_t)div_denom; }
    zero = (d == 0); divcnt = (divcnt & ~0x4000) | (zero ? 0x4000 : 0);
    int64_t q, r;
    if (zero) { q = n < 0 ? 1 : -1; r = n; }
    else if (d == -1 && n == INT64_MIN) { q = INT64_MIN; r = 0; }
    else { q = n / d; r = n % d; }
    if (mode == 0) { q = (int32_t)q; r = (int32_t)r; }
    div_res = (uint64_t)q; div_rem = (uint64_t)r;
}
void Console::sqrt_update() {
    uint64_t v = (sqrtcnt & 1) ? (((uint64_t)sqrt_param64[1] << 32) | sqrt_param64[0]) : sqrt_param64[0];
    uint64_t r = (uint64_t)std::sqrt((long double)v);
    while (r * r > v) r--; while ((r + 1) * (r + 1) <= v) r++;
    sqrt_res = (uint32_t)r;
}

uint16_t Console::io_r16(int cpu, uint32_t a) {
    uint32_t off = a & 0x1FFE;
    if (off >= 0xB0 && off < 0xE0) { int ch = (off - 0xB0) / 12, reg = (off - 0xB0) % 12; return reg == 10 ? dma[cpu][ch].ctrl : 0; }
    if (off >= 0x100 && off < 0x110) { Timer& t = timers[cpu][(off - 0x100) / 4]; return (off & 2) ? t.ctrl : t.counter; }
    if (cpu == 0 && off >= 0x280 && off < 0x2C0) {
        auto q = [&](uint64_t v, int sh) { return (uint16_t)(v >> sh); };
        switch (off) {
        case 0x280: return divcnt; case 0x290: case 0x292: case 0x294: case 0x296: return q(div_numer, (off - 0x290) * 8);
        case 0x298: case 0x29A: case 0x29C: case 0x29E: return q(div_denom, (off - 0x298) * 8);
        case 0x2A0: case 0x2A2: case 0x2A4: case 0x2A6: return q(div_res, (off - 0x2A0) * 8);
        case 0x2A8: case 0x2AA: case 0x2AC: case 0x2AE: return q(div_rem, (off - 0x2A8) * 8);
        case 0x2B0: return sqrtcnt; case 0x2B4: return (uint16_t)sqrt_res; case 0x2B6: return (uint16_t)(sqrt_res >> 16);
        case 0x2B8: case 0x2BA: return (uint16_t)(sqrt_param64[0] >> ((off - 0x2B8) * 8));
        case 0x2BC: case 0x2BE: return (uint16_t)(sqrt_param64[1] >> ((off - 0x2BC) * 8));
        }
    }
    switch (off) {
    case 0x004: return (dispstat[cpu] & 0xFFB8) | (io[cpu][4] & 7);
    case 0x006: return (uint16_t)vcount;
    case 0x130: return (uint16_t)(~keys & 0x3FF);
    case 0x136: if (cpu == 1) return (uint16_t)(0x007F & ~((keys >> 10) & 3) & ~(touch_down_ ? 0x40 : 0)); break;   // bit 6 = lapiz (0 = pulsado)
    case 0x1C0: if (cpu == 1) return (uint16_t)(spicnt_ & ~0x80); break;      // SPICNT: nunca ocupado (transferencia instantanea)
    case 0x1C2: if (cpu == 1) return spi_out_; break;
    case 0x180: return (uint16_t)((ipcsync_out[cpu ^ 1] & 0xF) | ((ipcsync_out[cpu] & 0xF) << 8) | ipcsync_cnt[cpu]);
    case 0x184: return ipc_cnt(cpu);
    case 0x1A0: card_reg_access[cpu][0x1A0]++; return (uint16_t)(auxspicnt & ~0x80);          // nunca ocupado
    case 0x1A2: return aux_out_;
    case 0x1A4: card_reg_access[cpu][0x1A4]++; return (uint16_t)(romctrl | (card_words.empty() ? 0 : (1u << 23)));
    case 0x1A6: return (uint16_t)((romctrl | (card_words.empty() ? 0 : (1u << 23))) >> 16);
    case 0x208: return (uint16_t)IME[cpu];
    case 0x210: return (uint16_t)IE[cpu]; case 0x212: return (uint16_t)(IE[cpu] >> 16);
    case 0x214: return (uint16_t)IF[cpu]; case 0x216: return (uint16_t)(IF[cpu] >> 16);
    case 0x240: if (cpu == 1) return (uint16_t)(wramcnt << 8); break;
    case 0x246: if (cpu == 0) return (uint16_t)(io[0][0x246] | (wramcnt << 8)); break;
    case 0x300: return io[cpu][0x300];
    }
    if (cpu == 1 && off >= 0x400 && off < 0x500 && (off & 0xF) == 2) { int ch = (off - 0x400) >> 4; return (uint16_t)((io[1][off] | (io[1][off + 1] << 8)) & 0x7FFF) | (snd_[ch].active ? 0x8000 : 0); }   // bit 15 = ocupado
    if (cpu == 0 && off == 0x600) return 0;                           // GXSTAT: cola vacia, motor no ocupado
    if (cpu == 0 && off == 0x602) return (uint16_t)(0x0600 | (gx_irq_mode_ << 14));
    if (cpu == 0 && (off < 0x70 || (off >= 0x1000 && off < 0x1070) || off == 0x60 || (off >= 0x330 && off < 0x3C0) || (off >= 0x604 && off < 0x700)))
        return (uint16_t)(io[0][off] | (io[0][off + 1] << 8));         // registros de video: valor almacenado
    if (off < 0x70) return 0;
    io_unknown[cpu][off]++;
    return (uint16_t)(io[cpu][off] | (io[cpu][off + 1] << 8));
}
uint32_t Console::io_r32(int cpu, uint32_t a) { return io_r16(cpu, a) | ((uint32_t)io_r16(cpu, a + 2) << 16); }

void Console::io_w8(int cpu, uint32_t a, uint8_t v) {
    uint32_t off = a & 0x1FFF;
    if (cpu == 0 && off == 0x247) { wramcnt = v & 3; return; }
    if (cpu == 0 && off >= 0x240 && off < 0x24A) { io[0][off] = v; rebuild_vram_map(); return; }
    if (off == 0x300) { io[cpu][0x300] = v; return; }
    if (off >= 0x214 && off < 0x218) { IF[cpu] &= ~((uint32_t)v << (8 * (off & 3))); return; }
    if (off >= 0x1A8 && off < 0x1B0) { card_cmd[off - 0x1A8] = v; return; }
    uint16_t old = io_r16(cpu, off & ~1u);
    uint16_t nv = (off & 1) ? (uint16_t)((old & 0xFF) | (v << 8)) : (uint16_t)((old & 0xFF00) | v);
    if (off >= 0x240 && off < 0x24A) { io[cpu][off] = v; return; }
    io_w16(cpu, off & ~1u, nv);
}

void Console::io_w32(int cpu, uint32_t a, uint32_t v) {
    uint32_t off = a & 0x1FFC;
    if (cpu == 0 && off >= 0x400 && off < 0x600) { if (off < 0x440) gpu3d.fifo_write(v); else gpu3d.port_write((off - 0x400) / 4, v); gx_irq_pending_ = true; gx_delay_ = 600; return; }
    if (cpu == 0 && off == 0x350) { gpu3d.set_clear_color(v); return; }
    if (cpu == 0 && off == 0x354) { gpu3d.set_clear_depth(v); return; }
    if (off == 0x188) { ipc_send(cpu, v); return; }
    if (off >= 0xB0 && off < 0xE0) {
        int ch = (off - 0xB0) / 12, reg = (off - 0xB0) % 12;
        if (reg == 0) { dma[cpu][ch].sad = v; return; } if (reg == 4) { dma[cpu][ch].dad = v; return; }
        if (reg == 8) { dma[cpu][ch].cnt = v & 0xFFFF; dma_write_ctrl(cpu, ch, (uint16_t)(v >> 16)); return; }
    }
    if (off >= 0x1A0 && off < 0x1C0) card_reg_access[cpu][off | 0x10000]++;
    if (off == 0x1A4) { romctrl = (v & 0x7F7FFFFF) | (v & 0x80000000); if (v & 0x80000000) card_start(cpu); return; }
    if (off == 0x1A8 || off == 0x1AC) { for (int i = 0; i < 4; i++) card_cmd[(off - 0x1A8) + i] = (v >> (8 * i)) & 0xFF; return; }   // el byte de menor direccion es el comando
    if (cpu == 0 && off == 0x290) { div_numer = (div_numer & ~0xFFFFFFFFull) | v; div_update(); return; }
    if (cpu == 0 && off == 0x294) { div_numer = (div_numer & 0xFFFFFFFFull) | ((uint64_t)v << 32); div_update(); return; }
    if (cpu == 0 && off == 0x298) { div_denom = (div_denom & ~0xFFFFFFFFull) | v; div_update(); return; }
    if (cpu == 0 && off == 0x29C) { div_denom = (div_denom & 0xFFFFFFFFull) | ((uint64_t)v << 32); div_update(); return; }
    if (cpu == 0 && off == 0x2B8) { sqrt_param64[0] = v; sqrt_update(); return; }
    if (cpu == 0 && off == 0x2BC) { sqrt_param64[1] = v; sqrt_update(); return; }
    if (off == 0x210) { IE[cpu] = v; return; }
    if (off == 0x214) { IF[cpu] &= ~v; if (v) if_clears[cpu][v]++; return; }
    io_w16(cpu, off, (uint16_t)v); io_w16(cpu, off + 2, (uint16_t)(v >> 16));
}

void Console::io_w16(int cpu, uint32_t a, uint16_t v) {
    uint32_t off = a & 0x1FFE;
    if (cpu == 0 && off >= 0x240 && off < 0x24A) { io[0][off] = v & 0xFF; if (off == 0x246) wramcnt = (v >> 8) & 3; else io[0][off + 1] = v >> 8; rebuild_vram_map(); return; }
    if (off >= 0xB0 && off < 0xE0) {
        int ch = (off - 0xB0) / 12, reg = (off - 0xB0) % 12; Dma& d = dma[cpu][ch];
        switch (reg) { case 0: d.sad = (d.sad & 0xFFFF0000u) | v; break; case 2: d.sad = (d.sad & 0xFFFFu) | ((uint32_t)v << 16); break;
                       case 4: d.dad = (d.dad & 0xFFFF0000u) | v; break; case 6: d.dad = (d.dad & 0xFFFFu) | ((uint32_t)v << 16); break;
                       case 8: d.cnt = v; break; case 10: dma_write_ctrl(cpu, ch, v); break; }
        return;
    }
    if (off >= 0x100 && off < 0x110) {
        Timer& t = timers[cpu][(off - 0x100) / 4];
        if (!(off & 2)) t.reload = v; else { bool was = t.ctrl & 0x80; t.ctrl = v & 0xC7; if (!was && (v & 0x80)) { t.counter = t.reload; t.acc = 0; } }
        return;
    }
    if (cpu == 0 && off >= 0x280 && off < 0x2C0) {
        if (off == 0x280) { divcnt = (divcnt & 0x4000) | (v & 3); div_update(); return; }
        if (off == 0x2B0) { sqrtcnt = v & 1; sqrt_update(); return; }
        auto setq = [&](uint64_t& q, int idx, uint16_t w) { q = (q & ~(0xFFFFull << (idx * 16))) | ((uint64_t)w << (idx * 16)); };
        if (off >= 0x290 && off < 0x298) { setq(div_numer, (off - 0x290) / 2, v); div_update(); return; }
        if (off >= 0x298 && off < 0x2A0) { setq(div_denom, (off - 0x298) / 2, v); div_update(); return; }
        if (off >= 0x2B8 && off < 0x2C0) { int i = (off - 0x2B8) / 4; int h = ((off - 0x2B8) & 2) ? 16 : 0;
            sqrt_param64[i] = (sqrt_param64[i] & ~(0xFFFFu << h)) | ((uint32_t)v << h); sqrt_update(); return; }
    }
    if (off >= 0x1A8 && off < 0x1B0) { card_cmd[off - 0x1A8] = v & 0xFF; card_cmd[off - 0x1A8 + 1] = v >> 8; return; }
    if (cpu == 0 && off >= 0x380 && off < 0x3C0) { gpu3d.set_toon((off - 0x380) / 2, v); io[0][off] = v & 0xFF; io[0][off + 1] = v >> 8; return; }
    if (cpu == 0 && off == 0x602) { gx_irq_mode_ = (v >> 14) & 3; gx_stat_writes++; gx_last_stat = v; if (gx_irq_mode_) { gx_irq_pending_ = true; gx_delay_ = 300; } return; }
    switch (off) {
    case 0x004: dispstat[cpu] = (dispstat[cpu] & 0x0047) | (v & 0xFFB8); return;
    case 0x180: ipcsync_out[cpu] = (v >> 8) & 0xF; ipcsync_cnt[cpu] = v & 0x4000;
                if ((v & 0x2000) && (ipcsync_cnt[cpu ^ 1] & 0x4000)) raise_irq(cpu ^ 1, 16); return;
    case 0x184: ipc_cnt_write(cpu, v); return;
    case 0x1A0: card_reg_access[cpu][0x1A0 | 0x10000]++; auxspicnt = v; return;
    case 0x1A2: if (auxspi_log && auxspi_log->size() < 3000) auxspi_log->push_back({auxspicnt, (uint8_t)v}); auxspi_transfer((uint8_t)v); return;
    case 0x1C0: if (cpu == 1) { spicnt_ = v & 0xCF03; return; } break;
    case 0x1C2: if (cpu == 1) { spi_transfer((uint8_t)v); return; } break;
    case 0x208: IME[cpu] = v & 1; return;
    case 0x210: IE[cpu] = (IE[cpu] & 0xFFFF0000u) | v; return; case 0x212: IE[cpu] = (IE[cpu] & 0xFFFFu) | ((uint32_t)v << 16); return;
    case 0x214: IF[cpu] &= ~(uint32_t)v; return; case 0x216: IF[cpu] &= ~((uint32_t)v << 16); return;
    }
    if (!(off < 0x70) && !(off >= 0x1000 && off < 0x1070) && !(off >= 0x330 && off < 0x3C0) && off != 0x60 && off != 0x130 && off != 0x132) io_unknown[cpu][off | 0x10000]++;
    io[cpu][off] = v & 0xFF; io[cpu][off + 1] = v >> 8;
    if (cpu == 1 && off >= 0x400 && off < 0x500 && (off & 0xF) < 4) snd_cnt_changed((off - 0x400) >> 4);
}

// ---------- temporizadores ----------
void Console::timer_overflow(int cpu, int t) {
    Timer& T = timers[cpu][t];
    T.counter = T.reload;
    if (T.ctrl & 0x40) raise_irq(cpu, 3 + t);
    if (t < 3) { Timer& N = timers[cpu][t + 1];
        if ((N.ctrl & 0x80) && (N.ctrl & 4)) { if (N.counter == 0xFFFF) timer_overflow(cpu, t + 1); else N.counter++; } }
}
void Console::tick_timers(int cpu, uint32_t units) {
    static const uint32_t div[4] = {1, 64, 256, 1024};
    for (int t = 0; t < 4; t++) {
        Timer& T = timers[cpu][t];
        if (!(T.ctrl & 0x80) || ((T.ctrl & 4) && t > 0)) continue;
        T.acc += units; uint32_t d = div[T.ctrl & 3];
        while (T.acc >= d) { T.acc -= d; if (T.counter == 0xFFFF) timer_overflow(cpu, t); else T.counter++; }
    }
}

// ---------- DMA ----------
void Console::dma_write_ctrl(int cpu, int ch, uint16_t v) {
    Dma& d = dma[cpu][ch]; bool was = d.ctrl & 0x8000; d.ctrl = v;
    if (!(v & 0x8000)) return;
    if (!was) {
        if (dma_log.size() > 400) dma_log.erase(dma_log.begin(), dma_log.begin() + 200);
        dma_log.push_back({cpu, ch, d.sad, d.dad, d.cnt, v, dma_seq_++, (cpu ? cpu7 : cpu9).pc(), (cpu ? cpu7 : cpu9).r[14]});
        d.isad = d.sad; d.idad = d.dad;
        uint32_t n = d.cnt & 0xFFFF; if (cpu == 0) n |= (uint32_t)(v & 0x1F) << 16; else if (ch != 3) n &= 0x3FFF;
        if (n == 0) n = (cpu == 0) ? 0x200000 : (ch == 3 ? 0x10000 : 0x4000);
        d.icnt = n;
    }
    int timing = (cpu == 0) ? (v >> 11) & 7 : (v >> 12) & 3;
    d.armed_card = (cpu == 0 && timing == 5) || (cpu == 1 && timing == 2);
    if ((timing == 0 || (cpu == 0 && timing == 7)) && !was) dma_run(cpu, ch);
}
void Console::dma_run(int cpu, int ch) {
    Dma& d = dma[cpu][ch];
    bool w32 = d.ctrl & 0x400; int sz = w32 ? 4 : 2; int dc = (d.ctrl >> 5) & 3, sc = (d.ctrl >> 7) & 3;
    for (uint32_t i = 0; i < d.icnt; i++) {
        uint32_t v = mem_read(cpu, d.isad & ~(uint32_t)(sz - 1), sz);
        mem_write(cpu, d.idad & ~(uint32_t)(sz - 1), v, sz);
        if (sc == 0) d.isad += sz; else if (sc == 1) d.isad -= sz;
        if (dc == 0 || dc == 3) d.idad += sz; else if (dc == 1) d.idad -= sz;
    }
    int timing = (cpu == 0) ? (d.ctrl >> 11) & 7 : (d.ctrl >> 12) & 3;
    if ((d.ctrl & 0x200) && timing != 0) {
        uint32_t n = d.cnt & 0xFFFF; if (cpu == 0) n |= (uint32_t)(d.ctrl & 0x1F) << 16; else if (ch != 3) n &= 0x3FFF;
        if (n == 0) n = (cpu == 0) ? 0x200000 : (ch == 3 ? 0x10000 : 0x4000);
        d.icnt = n; if (dc == 3) d.idad = d.dad;
    } else d.ctrl &= ~0x8000;
    if (d.ctrl & 0x4000 || (!(d.ctrl & 0x8000) && (d.ctrl & 0x4000))) raise_irq(cpu, 8 + ch);
}
void Console::dma_vblank(int cpu) {
    for (int ch = 0; ch < 4; ch++) { Dma& d = dma[cpu][ch]; if (!(d.ctrl & 0x8000)) continue;
        int timing = (cpu == 0) ? (d.ctrl >> 11) & 7 : (d.ctrl >> 12) & 3; if (timing == 1) dma_run(cpu, ch); }
}
void Console::dma_hblank(int cpu) {
    if (cpu != 0 || vcount >= 192) return;
    for (int ch = 0; ch < 4; ch++) { Dma& d = dma[0][ch]; if ((d.ctrl & 0x8000) && ((d.ctrl >> 11) & 7) == 2) dma_run(0, ch); }
}

// ---------- cartucho ----------
void Console::card_start(int cpu) {
    card_owner = cpu; card_words.clear();
    static const uint32_t sizes[8] = {0, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000, 4};
    uint32_t bytes = sizes[(romctrl >> 24) & 7];
    uint8_t cmd = card_cmd[0]; card_cmd_count[cmd]++;
    std::vector<uint8_t> data(bytes, 0xFF);
    if (cmd == 0xB7) {
        uint32_t addr = ((uint32_t)card_cmd[1] << 24) | (card_cmd[2] << 16) | (card_cmd[3] << 8) | card_cmd[4];
        for (uint32_t i = 0; i < bytes; i++) data[i] = (addr + i < rom.size()) ? rom[addr + i] : 0xFF;
        card_blocks++;
    } else if (cmd == 0x00) { for (uint32_t i = 0; i < bytes; i++) data[i] = (i % 0x1000) < rom.size() ? rom[i % 0x1000] : 0xFF; }
    else if (cmd == 0x90 || cmd == 0xB8) { for (uint32_t i = 0; i < bytes; i += 4) { uint32_t id = 0x00001FC2; std::memcpy(&data[i], &id, std::min<uint32_t>(4, bytes - i)); } }
    for (uint32_t i = 0; i + 3 < bytes + 3 && i < bytes; i += 4) { uint32_t w; std::memcpy(&w, &data[i], std::min<uint32_t>(4, bytes - i)); card_words.push_back(w); }
    if (bytes == 0) { romctrl &= ~((1u << 31) | (1u << 23)); if (auxspicnt & 0x4000) raise_irq(cpu, 19); return; }
    // DMA con arranque por cartucho: cada palabra dispara una transferencia
    for (int ch = 0; ch < 4; ch++) {
        Dma& d = dma[cpu][ch];
        if ((d.ctrl & 0x8000) && d.armed_card) {
            while (!card_words.empty() && (d.ctrl & 0x8000)) dma_run(cpu, ch);
            card_words.clear(); romctrl &= ~((1u << 31) | (1u << 23)); if (auxspicnt & 0x4000) raise_irq(cpu, 19);
            return;
        }
    }
}

// ---------- memoria de guardado: EEPROM de 64 Kbit por AUXSPI ----------
void Console::auxspi_transfer(uint8_t out) {
    if (!(auxspicnt & 0x8000)) return;                                  // AUXSPI desactivado
    bool hold = auxspicnt & 0x40; uint8_t in = 0xFF;
    const uint32_t size = (uint32_t)backup_.size();
    if (bk_state_ == 0) {                                               // byte de comando
        bk_cmd_ = out; bk_addr_ = 0; in = 0xFF;
        switch (out) {
        case 0x06: bk_wel_ = true; break;                               // WREN
        case 0x04: bk_wel_ = false; break;                              // WRDI
        case 0x05: bk_state_ = 30; break;                               // RDSR
        case 0x01: bk_state_ = 31; break;                               // WRSR (se ignora el valor)
        case 0x03: case 0x02: bk_state_ = 1; break;                     // READ / WRITE: direccion de 2 bytes
        default: break;                                                 // otros (p. ej. 0x9F, ID de flash): 0xFF = "no soportado"
        }
    } else if (bk_state_ == 1 || bk_state_ == 2) {
        bk_addr_ = (bk_addr_ << 8) | out;
        if (++bk_state_ == 3) { bk_addr_ %= size; bk_state_ = (bk_cmd_ == 0x03) ? 10 : (bk_wel_ ? 40 : 0); }
    } else if (bk_state_ == 10) { in = backup_[bk_addr_]; bk_addr_ = (bk_addr_ + 1) % size; }
    else if (bk_state_ == 40) {                                         // escritura dentro de una pagina de 32 bytes
        backup_[bk_addr_] = out; backup_dirty = true; bk_addr_ = (bk_addr_ & ~31u) | ((bk_addr_ + 1) & 31u);
    } else if (bk_state_ == 30) { in = (uint8_t)(bk_wel_ ? 0x02 : 0x00); }   // bit1 = WEL, bit0 = WIP (nunca ocupado)
    else if (bk_state_ == 31) { in = 0xFF; }
    if (!hold) { if (bk_state_ == 40) bk_wel_ = false; bk_state_ = 0; }
    aux_out_ = in;
}

// ---------- sonido ----------
static const int kAdpcmIndex[8] = {-1, -1, -1, -1, 2, 4, 6, 8};
static const int kAdpcmStep[89] = {7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};

void Console::snd_cnt_changed(int ch) {
    uint32_t base = 0x400 + ch * 16;
    uint32_t cnt = io[1][base] | (io[1][base + 1] << 8) | (io[1][base + 2] << 16) | ((uint32_t)io[1][base + 3] << 24);
    SndCh& s = snd_[ch];
    if (!(cnt >> 31)) { s.active = false; return; }
    if (s.active) return;                                                       // ya sonando
    uint32_t sad = io[1][base + 4] | (io[1][base + 5] << 8) | (io[1][base + 6] << 16) | ((uint32_t)io[1][base + 7] << 24);
    s.sad = sad & 0x07FFFFFC; s.loop = io[1][base + 10] | (io[1][base + 11] << 8);
    s.len = (io[1][base + 12] | (io[1][base + 13] << 8) | (io[1][base + 14] << 16)) & 0x3FFFFF;
    s.fmt = (cnt >> 29) & 3; s.pos = 0; s.last_i = 0; s.lfsr = 0x7FFF; s.active = true;
    if (s.fmt == 2) {                                                           // IMA-ADPCM: cabecera de 4 bytes (valor inicial + indice)
        uint32_t hd = mem_read(1, s.sad, 4); s.pred = (int16_t)(hd & 0xFFFF); s.idx = std::min<int>(88, (hd >> 16) & 127);
        s.lpred = s.pred; s.lidx = s.idx; s.next_nib = 0; s.cur = s.pred;
    }
}

void Console::snd_mix_one() {
    int32_t L = 0, R = 0;
    uint32_t master = io[1][0x500] | (io[1][0x501] << 8);
    if (master & 0x8000) {
        for (int ch = 0; ch < 16; ch++) {
            SndCh& s = snd_[ch]; if (!s.active) continue;
            uint32_t base = 0x400 + ch * 16;
            uint32_t cnt = io[1][base] | (io[1][base + 1] << 8) | (io[1][base + 2] << 16) | ((uint32_t)io[1][base + 3] << 24);
            uint32_t tmr = io[1][base + 8] | (io[1][base + 9] << 8); uint32_t period = 0x10000 - tmr; if (period < 1) period = 1;
            double step = 512.0 / period;                                       // muestras del canal por muestra de salida
            int32_t smp = 0; int64_t i = (int64_t)s.pos;
            bool psg = (ch >= 8 && s.fmt == 3);
            if (psg && ch >= 14) {                                              // ruido: LFSR de 15 bits
                for (int64_t k = s.last_i; k < i && k < s.last_i + 16; k++) { bool c = s.lfsr & 1; s.lfsr >>= 1; if (c) s.lfsr ^= 0x6000; }
                s.last_i = i; smp = (s.lfsr & 1) ? -0x7FFF : 0x7FFF;
            } else if (psg) {                                                   // onda cuadrada: ciclo de trabajo 12,5 % .. 87,5 %
                uint32_t duty = (cnt >> 24) & 7; smp = (duty < 7 && (uint32_t)(i & 7) >= 7 - duty) ? 0x7FFF : -0x7FFF;
            } else {
                uint32_t total, lstart;
                if (s.fmt == 0) { total = (s.loop + s.len) * 4; lstart = s.loop * 4; }
                else if (s.fmt == 1) { total = (s.loop + s.len) * 2; lstart = s.loop * 2; }
                else { total = (s.loop + s.len) * 8 - 8; lstart = s.loop * 8 - 8; }
                if ((uint32_t)i >= total) {
                    if (((cnt >> 27) & 3) == 2) { s.active = false; continue; }  // una sola vez: termina
                    s.pos = (double)lstart + (s.pos - (double)total); i = (int64_t)s.pos; if (i < 0) i = 0;
                    if (s.fmt == 2) { s.pred = s.lpred; s.idx = s.lidx; s.next_nib = lstart; }
                }
                if (s.fmt == 0) smp = (int8_t)mem_read(1, s.sad + (uint32_t)i, 1) * 256;
                else if (s.fmt == 1) smp = (int16_t)mem_read(1, (s.sad + (uint32_t)i * 2) & ~1u, 2);
                else {
                    while (s.next_nib <= i) {
                        if ((uint32_t)s.next_nib == lstart) { s.lpred = s.pred; s.lidx = s.idx; }
                        uint32_t by = mem_read(1, s.sad + 4 + (uint32_t)(s.next_nib >> 1), 1); int nib = (s.next_nib & 1) ? (by >> 4) & 15 : by & 15;
                        int stp = kAdpcmStep[s.idx], df = stp >> 3; if (nib & 1) df += stp >> 2; if (nib & 2) df += stp >> 1; if (nib & 4) df += stp;
                        s.pred += (nib & 8) ? -df : df; s.pred = std::max(-32768, std::min(32767, s.pred));
                        s.idx = std::max(0, std::min(88, s.idx + kAdpcmIndex[nib & 7])); s.cur = s.pred; s.next_nib++;
                    }
                    smp = s.cur;
                }
                s.pos += step;
            }
            if (psg) s.pos += step;
            static const int sh[4] = {0, 1, 2, 4};
            smp >>= sh[(cnt >> 8) & 3];
            int32_t v = smp * (int32_t)(cnt & 127) / 128; int pan = (cnt >> 16) & 127;
            L += v * (128 - pan) / 128; R += v * (pan + 1) / 128;
        }
    }
    int32_t mv = master & 127; L = L * mv / 192; R = R * mv / 192;              // volumen maestro y holgura para no saturar con muchos canales
    L = std::max(-32768, std::min(32767, L)); R = std::max(-32768, std::min(32767, R));
    size_t h = a_head_.load(std::memory_order_relaxed), t = a_tail_.load(std::memory_order_acquire), cap = aring_.size() / 2;
    if (((h + 1) % cap) == t) return;                                           // cola llena: se descarta la muestra
    aring_[h * 2] = (int16_t)L; aring_[h * 2 + 1] = (int16_t)R; a_head_.store((h + 1) % cap, std::memory_order_release);
}

size_t Console::audio_read(int16_t* out, size_t max_frames) {
    size_t cap = aring_.size() / 2, t = a_tail_.load(std::memory_order_relaxed), h = a_head_.load(std::memory_order_acquire), n = 0;
    while (t != h && n < max_frames) { out[n * 2] = aring_[t * 2]; out[n * 2 + 1] = aring_[t * 2 + 1]; t = (t + 1) % cap; n++; }
    a_tail_.store(t, std::memory_order_release);
    return n;
}

// ---------- SPI (solo ARM7) ----------
void Console::spi_transfer(uint8_t out) {
    if (!(spicnt_ & 0x8000)) return;                                    // SPI desactivado
    int dev = (spicnt_ >> 8) & 3; bool hold = spicnt_ & 0x800; uint8_t in = 0;
    spi_transfers[dev == 3 ? 0 : dev]++;
    switch (dev) {
    case 0:                                                             // chip de energia
        if (pm_state_ == 0) { pm_reg_ = out & 7; pm_state_ = (out & 0x80) ? 1 : 2; in = 0; }
        else { if (pm_state_ == 1) in = pm_regs_[pm_reg_]; else pm_regs_[pm_reg_] = out; pm_state_ = 0; if (hold) pm_state_ = 0; }
        break;
    case 1:                                                             // firmware: flash serie estandar (ST M45PE / similar)
        if (fw_state_ == 0) {                                           // byte de comando
            fw_cmd_ = out; fw_addr_ = 0; in = 0;
            switch (out) {
            case 0x06: fw_wel_ = true; fw_state_ = 0; break;            // WREN: habilitar escritura
            case 0x04: fw_wel_ = false; fw_state_ = 0; break;           // WRDI
            case 0x05: fw_state_ = 30; break;                           // RDSR: registro de estado
            case 0x9F: fw_state_ = 20; break;                           // identificador JEDEC: 20 40 12
            case 0x03: case 0x0B: case 0x0A: case 0x02: case 0xDB: case 0xD8: fw_state_ = 1; break;   // llevan direccion de 3 bytes
            default: fw_state_ = 0; break;
            }
        }
        else if (fw_state_ >= 1 && fw_state_ <= 3) {
            fw_addr_ = (fw_addr_ << 8) | out;
            if (++fw_state_ == 4) {
                fw_addr_ %= fw_.size();
                if (fw_cmd_ == 0x03) { fw_state_ = 10; fw_reads[fw_addr_]++; }
                else if (fw_cmd_ == 0x0B) fw_state_ = 11;                               // lectura rapida: byte ficticio primero
                else if (fw_cmd_ == 0x0A || fw_cmd_ == 0x02) { if (fw_wel_) { fw_state_ = 40; if (fw_cmd_ == 0x0A) std::fill(fw_.begin() + (fw_addr_ & ~0xFFu), fw_.begin() + (fw_addr_ & ~0xFFu) + 0x100, 0xFF); } else fw_state_ = 0; }
                else if (fw_cmd_ == 0xDB) { if (fw_wel_) std::fill(fw_.begin() + (fw_addr_ & ~0xFFu), fw_.begin() + (fw_addr_ & ~0xFFu) + 0x100, 0xFF); fw_wel_ = false; fw_state_ = 0; }
                else if (fw_cmd_ == 0xD8) { if (fw_wel_) std::fill(fw_.begin() + (fw_addr_ & ~0xFFFFu), fw_.begin() + (fw_addr_ & ~0xFFFFu) + 0x10000, 0xFF); fw_wel_ = false; fw_state_ = 0; }
            }
        }
        else if (fw_state_ == 11) { fw_state_ = 10; }
        else if (fw_state_ == 10) { in = fw_[fw_addr_ % fw_.size()]; fw_addr_++; }
        else if (fw_state_ == 40) { fw_[fw_addr_] = (uint8_t)(fw_[fw_addr_] & out); fw_addr_ = (fw_addr_ & ~0xFFu) | ((fw_addr_ + 1) & 0xFF); }   // se programa dentro de la pagina de 256 bytes
        else if (fw_state_ >= 20 && fw_state_ < 23) { static const uint8_t id[3] = {0x20, 0x40, 0x12}; in = id[fw_state_ - 20]; fw_state_++; }
        else if (fw_state_ == 30) { in = fw_wel_ ? 0x02 : 0x00; }               // bit1 = WEL, bit0 = WIP (nunca ocupado: instantaneo)
        break;
    default:                                                            // pantalla tactil (sin pulsacion)
        if (out & 0x80) {                                                   // byte de control: canal en bits 4-6
            int ch = (out >> 4) & 7; uint16_t v = 0;
            // calibracion lineal de los ajustes por defecto: ADC 0x020..0xFE0 en X (0..255) y 0x020..0xBE0 en Y (0..191)
            switch (ch) {
            case 1: v = touch_down_ ? (uint16_t)(0x20 + touch_y_ * (0xBE0 - 0x20) / 191) : 0; break;     // Y
            case 5: v = touch_down_ ? (uint16_t)(0x20 + touch_x_ * (0xFE0 - 0x20) / 255) : 0; break;     // X
            case 3: v = touch_down_ ? 0x800 : 0; break;                                                    // Z1 (presion)
            case 4: v = touch_down_ ? 0x700 : 0xFFF; break;                                                // Z2
            case 0: v = 0x700; break;                                                                      // temperatura
            default: v = 0x800; break;
            }
            touch_val_ = v; touch_state_ = 1; in = 0; }
        else if (touch_state_ == 1) { in = (uint8_t)((touch_val_ >> 5) & 0x7F); touch_state_ = 2; }
        else if (touch_state_ == 2) { in = (uint8_t)((touch_val_ << 3) & 0xFF); touch_state_ = 0; }
        break;
    }
    if (!hold) { pm_state_ = 0; if (dev == 1 && (fw_cmd_ == 0x0A || fw_cmd_ == 0x02) && fw_state_ == 40) fw_wel_ = false; if (dev == 1) { if (fw_cmd_ != 0x06 && fw_cmd_ != 0x04 && fw_cmd_ != 0x05) {} fw_state_ = 0; } if (dev != 2) touch_state_ = 0; }
    spi_out_ = in;
    if (spicnt_ & 0x4000) raise_irq(1, 23);
}

// ---------- tiempo y pantalla ----------
void Console::hblank_start() {
    for (int c = 0; c < 2; c++) { dispstat[c] |= 2; if (dispstat[c] & 0x10) raise_irq(c, 1); }
    dma_hblank(0);
}
void Console::scanline_step() {
    vcount++; if (vcount >= 263) { vcount = 0; frame++; }
    for (int c = 0; c < 2; c++) {
        dispstat[c] &= ~2;
        bool vbl = vcount >= 192 && vcount <= 261;
        if (vbl) dispstat[c] |= 1; else dispstat[c] &= ~1;
        uint32_t lyc = (dispstat[c] >> 8) | ((dispstat[c] & 0x80) << 1);
        if (vcount == lyc) { dispstat[c] |= 4; if (dispstat[c] & 0x20) raise_irq(c, 2); } else dispstat[c] &= ~4;
        if (vcount == 192) { if (dispstat[c] & 0x8) raise_irq(c, 0); dma_vblank(c); if (c == 0 && !skip_render) render_frame(); }
    }
}

void Console::run_units(uint64_t units) {
    static const uint32_t kDiv[4] = {1, 64, 256, 1024};
    for (uint64_t u = 0; u < units;) {
        // Cola de geometria: en este modelo se vacia al instante. El aviso queda pendiente hasta que el juego
        // lo habilita en IE (el SDK lo habilita DESPUES de enviar los comandos, cuando en una consola real aun se estaba vaciando).
        if (gx_delay_) { gx_delay_--; }
        else if (gx_irq_pending_ && gx_irq_mode_ && (IE[0] & (1u << 21)) && !(IF[0] & (1u << 21))) { raise_irq(0, 21); gx_raised++; gx_irq_pending_ = false; }
        update_irq_lines();

        uint64_t n = 1;
        if (cpu9.halted && cpu7.halted) {
            // Las dos CPU duermen: no ejecutan nada hasta el siguiente evento. Se salta directamente a el.
            n = units - u;
            n = std::min<uint64_t>(n, (line_cycles < 1536 ? 1536u : 2130u) - line_cycles);       // proximo hblank / fin de linea
            for (int c = 0; c < 2; c++) for (int t = 0; t < 4; t++) {
                const Timer& T = timers[c][t];
                if (!(T.ctrl & 0x80) || ((T.ctrl & 4) && t > 0)) continue;
                uint64_t rem = (uint64_t)(0x10000 - T.counter) * kDiv[T.ctrl & 3] - T.acc;      // unidades hasta el desbordamiento
                if (rem >= 1) n = std::min<uint64_t>(n, rem);
            }
            if (gx_delay_) n = std::min<uint64_t>(n, gx_delay_ + 1u);
            if (n < 1) n = 1;
            if (n > 1 && gx_delay_) gx_delay_ -= (uint32_t)std::min<uint64_t>(gx_delay_, n - 1);
        } else {
            if (!cpu9.halted) { cpu9.step(); cpu9.step(); }
            if (!cpu7.halted) cpu7.step();
            if ((++steps_ & 0x3FF) == 0) { pc_samples[0][cpu9.pc()]++; pc_samples[1][cpu7.pc()]++; }
        }
        tick_timers(0, (uint32_t)n); tick_timers(1, (uint32_t)n);
        snd_acc_ += (uint32_t)n; while (snd_acc_ >= 1024) { snd_acc_ -= 1024; snd_mix_one(); }   // una muestra cada 1024 ciclos de bus (32728 Hz)
        line_cycles += (uint32_t)n;
        if (line_cycles == 1536) hblank_start();
        if (line_cycles >= 2130) { line_cycles = 0; scanline_step(); }
        u += n;
    }
}

// ---------- BIOS por software ----------
void Console::make_bios_stubs() {
    auto put = [](uint8_t* b, uint32_t off, uint32_t w) { std::memcpy(b + off, &w, 4); };
    for (uint32_t o = 0; o < 0x20; o += 4) { put(bios9, o, 0xEAFFFFFE); put(bios7, o, 0xEAFFFFFE); }   // vectores: salto a si mismos
    put(bios9, 0x18, 0xEA000000); put(bios7, 0x18, 0xEA000000);                                          // IRQ -> 0x20
    const uint32_t h9[] = {0xE92D500F, 0xEE190F11, 0xE1A00620, 0xE1A00600, 0xE2800901, 0xE28FE000, 0xE510F004, 0xE8BD500F, 0xE25EF004};
    const uint32_t h7[] = {0xE92D500F, 0xE3A00301, 0xE28FE000, 0xE510F004, 0xE8BD500F, 0xE25EF004};
    for (size_t i = 0; i < sizeof h9 / 4; i++) put(bios9, 0x20 + 4 * i, h9[i]);
    for (size_t i = 0; i < sizeof h7 / 4; i++) put(bios7, 0x20 + 4 * i, h7[i]);
}

static uint16_t crc16_fw(const uint8_t* p, int n) {
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < n; i++) { crc ^= p[i]; for (int k = 0; k < 8; k++) { bool l = crc & 1; crc >>= 1; if (l) crc ^= 0xA001; } }
    return crc;
}
void Console::make_firmware() {
    fw_.assign(0x40000, 0xFF);
    uint8_t* h = fw_.data(); std::memset(h, 0, 0x200);
    h[0x20] = 0xC0; h[0x21] = 0x7F;                                  // desplazamiento de los ajustes de usuario: 0x3FE00 / 8
    const uint8_t mac[6] = {0x00, 0x09, 0xBF, 0x12, 0x34, 0x56}; std::memcpy(h + 0x36, mac, 6);
    h[0x3C] = 0xFE; h[0x3D] = 0x1F;                                  // canales de WiFi habilitados
    for (int copy = 0; copy < 2; copy++) {                           // dos copias de los ajustes de usuario
        uint8_t* u = fw_.data() + 0x3FE00 + copy * 0x100; std::memset(u, 0, 0x100);
        std::memcpy(u, main_ram + 0x3FFC80, 0x70);                   // mismos datos que ve el juego en la RAM compartida
        u[0x70] = (uint8_t)(copy == 0 ? 1 : 0); u[0x71] = 0;
        uint16_t c = crc16_fw(u, 0x70); u[0x72] = (uint8_t)c; u[0x73] = (uint8_t)(c >> 8);
    }
}
Console::Console() { make_bios_stubs(); }

bool Console::load_rom(const std::vector<uint8_t>& r) {
    if (r.size() < 0x200) return false;
    rom = r;
    auto rd32 = [&](uint32_t o) { uint32_t v; std::memcpy(&v, &rom[o], 4); return v; };
    uint32_t a9o = rd32(0x20), a9e = rd32(0x24), a9l = rd32(0x28), a9s = rd32(0x2C);
    uint32_t a7o = rd32(0x30), a7e = rd32(0x34), a7l = rd32(0x38), a7s = rd32(0x3C);
    if ((uint64_t)a9o + a9s > rom.size() || (uint64_t)a7o + a7s > rom.size()) return false;
    // ---- Estado que deja la BIOS real en la RAM compartida (0x027FF800-0x027FFFFF) antes de saltar al juego ----
    auto put16 = [&](uint32_t off, uint16_t v) { std::memcpy(main_ram + 0x3FF000 + (off & 0xFFF), &v, 2); };
    auto put32 = [&](uint32_t off, uint32_t v) { std::memcpy(main_ram + 0x3FF000 + (off & 0xFFF), &v, 4); };
    std::memcpy(main_ram + 0x3FFE00, &rom[0], 0x200);                        // cabecera de la ROM en HW_ROM_HEADER_BUF (0x027FFE00)
    std::memcpy(main_ram + 0x3FFA80, &rom[0], 0x160);                        // y la copia temporal de la BIOS (0x027FFA80), que FS_Init vuelve a copiar
    uint32_t chip = 0x00001FC2;                                              // ID del chip del cartucho (copias de la BIOS)
    put32(0x800, chip); put32(0x804, chip); put32(0xC00, chip); put32(0xC04, chip);
    put16(0x808, (uint16_t)(rom[0x15E] | (rom[0x15F] << 8)));                // CRC de la cabecera
    put16(0x80A, (uint16_t)(rom[0x6C] | (rom[0x6D] << 8)));                  // CRC del area segura
    put16(0xC40, 1);                                                         // OS_GetBootType(): 1 = arrancado desde el cartucho
    // Ajustes de usuario por defecto (copia del firmware en 0x027FFC80): ingles, calibracion tactil lineal
    uint8_t* us = main_ram + 0x3FFC80; std::memset(us, 0, 0x70);
    us[0] = 5; us[2] = 0; us[3] = 1; us[4] = 1;                              // version, color, cumpleanos (1/1)
    const char* nick = "Sonic"; for (int i = 0; nick[i]; i++) { us[6 + 2 * i] = nick[i]; } us[0x1A] = 5;
    uint16_t calib[4] = {0x0020, 0x0020, 0x0FE0, 0x0BE0}; std::memcpy(us + 0x58, &calib[0], 4); us[0x5C] = 0; us[0x5D] = 0;
    std::memcpy(us + 0x5E, &calib[2], 4); us[0x62] = 255; us[0x63] = 191; us[0x64] = 1;   // idioma 1 = ingles
    for (uint32_t i = 0; i < a9s; i++) if (uint8_t* p = ptr(0, a9l + i, true)) *p = rom[a9o + i];
    for (uint32_t i = 0; i < a7s; i++) if (uint8_t* p = ptr(1, a7l + i, true)) *p = rom[a7o + i];
    // Sonda de fin de arranque: el crt0 descomprime el ARM9 HACIA ATRAS; lo ultimo que escribe son los bytes de direccion
    // mas baja de la parte comprimida. Calculamos la imagen descomprimida y recordamos esos bytes.
    booted_ = false; boot_probe_addr_ = 0;
    {
        std::vector<uint8_t> ref;
        if (a9s >= 8 && blz_decompress(&rom[a9o], a9s, ref) && ref.size() > a9s) {
            uint32_t enc = rd32(a9o + a9s - 8) & 0xFFFFFF, stop = a9s - enc;
            if (stop + 32 <= ref.size()) { boot_probe_addr_ = a9l + stop; std::memcpy(boot_probe_, &ref[stop], 32); }
        } else booted_ = true;                                               // no estaba comprimido
    }
    make_firmware();
    update_fast();
    cpu9.reset(a9e); cpu9.set_cpsr(0xDF, 1);                                  // modo SYS, IRQ desactivadas
    cpu7.reset(a7e); cpu7.set_cpsr(0xDF, 1);
    return true;
}

bool Console::arm9_booted() {
    if (booted_) return true;
    if (!boot_probe_addr_) return false;
    for (int i = 0; i < 32; i++) if (bus9.read8(boot_probe_addr_ + i) != boot_probe_[i]) return false;
    booted_ = true;                                                          // se queda fijado: el juego puede reescribir esos datos despues
    return true;
}

// ---------- HLE de BIOS: SWI ----------
bool Console::hle_swi(int cpu, Cpu& c, uint32_t n) {
    ConsoleBus& bus = cpu == 0 ? bus9 : bus7;
    auto rd8 = [&](uint32_t a) { return bus.read8(a) & 0xFFu; };
    uint32_t flags_addr = (cpu == 0) ? dtcm_base + 0x3FF8 : 0x0380FFF8;
    if (!c.thumb) n >>= 16;                                                   // en ARM el numero va en los bits 23:16
    switch (n) {
    case 0x03: { uint32_t k = c.r[0]; while (k--) {} return true; }           // WaitByLoop: sin espera real
    case 0x04: case 0x05: {                                                   // IntrWait / VBlankIntrWait
        uint32_t discard = (n == 5) ? 1 : c.r[0], mask = (n == 5) ? 1 : c.r[1];
        if (!c.hle_intrwait_started) { if (discard) bus.write32(flags_addr, bus.read32(flags_addr) & ~mask); c.hle_intrwait_started = true; IME[cpu] = 1; }
        uint32_t f = bus.read32(flags_addr);
        if (f & mask) { bus.write32(flags_addr, f & ~mask); c.hle_intrwait_started = false; return true; }
        c.halted = true; c.jump(c.pc()); return true;                         // reintentar la SWI al despertar
    }
    case 0x06: c.halted = true; return true;                                  // Halt
    case 0x09: { int32_t a = (int32_t)c.r[0], b = (int32_t)c.r[1]; if (b == 0) return true;
        int32_t q = (a == INT32_MIN && b == -1) ? INT32_MIN : a / b, r = (a == INT32_MIN && b == -1) ? 0 : a % b;
        c.r[0] = (uint32_t)q; c.r[1] = (uint32_t)r; c.r[3] = (uint32_t)(q < 0 ? -(int64_t)q : q); return true; }
    case 0x0B: { uint32_t src = c.r[0], dst = c.r[1], len = c.r[2] & 0x1FFFFF; bool fixed = c.r[2] & (1u << 24), w32 = c.r[2] & (1u << 26);
        uint32_t sz = w32 ? 4 : 2; uint32_t v = 0;
        for (uint32_t i = 0; i < len; i++) { if (!fixed || i == 0) v = (sz == 4) ? bus.read32(src) : bus.read16(src);
            if (sz == 4) bus.write32(dst, v); else bus.write16(dst, v); if (!fixed) src += sz; dst += sz; } return true; }
    case 0x0C: { uint32_t src = c.r[0], dst = c.r[1], len = (c.r[2] & 0x1FFFFF); bool fixed = c.r[2] & (1u << 24); uint32_t v = 0;
        len = (len + 7) & ~7u;
        for (uint32_t i = 0; i < len; i++) { if (!fixed || i == 0) v = bus.read32(src); bus.write32(dst, v); if (!fixed) src += 4; dst += 4; } return true; }
    case 0x0D: { uint32_t v = c.r[0]; uint32_t r = (uint32_t)std::sqrt((double)v); while ((uint64_t)r * r > v) r--; while ((uint64_t)(r + 1) * (r + 1) <= v) r++; c.r[0] = r; return true; }
    case 0x0E: { uint32_t crc = c.r[0] & 0xFFFF, src = c.r[1], len = c.r[2]; static const uint16_t tb[8] = {0xC0C1, 0xC181, 0xC301, 0xC601, 0xCC01, 0xD801, 0xF001, 0xA001};
        for (uint32_t i = 0; i < len; i += 2) { uint32_t w = bus.read16(src + i); for (int b = 0; b < 16; b += 8) { crc ^= (w >> b) & 0xFF;
            for (int k = 0; k < 8; k++) { bool lsb = crc & 1; crc >>= 1; if (lsb) crc ^= 0xA001; } } (void)tb; }
        c.r[0] = crc; return true; }
    case 0x0F: c.r[0] = 0; return true;                                       // IsDebugger
    case 0x11: case 0x12: {                                                   // LZ77
        uint32_t src = c.r[0], dst = c.r[1]; uint32_t hdr = bus.read32(src); src += 4; uint32_t size = hdr >> 8; uint32_t out = 0; std::vector<uint8_t> buf(size);
        while (out < size) { uint32_t fl = rd8(src++); for (int b = 0; b < 8 && out < size; b++, fl <<= 1) {
            if (fl & 0x80) { uint32_t b1 = rd8(src++), b2 = rd8(src++); uint32_t len = (b1 >> 4) + 3, disp = (((b1 & 15) << 8) | b2) + 1;
                for (uint32_t k = 0; k < len && out < size; k++) { buf[out] = out >= disp ? buf[out - disp] : 0; out++; } }
            else buf[out++] = (uint8_t)rd8(src++); } }
        if (n == 0x11) for (uint32_t i = 0; i < size; i++) bus.write8(dst + i, buf[i]);
        else for (uint32_t i = 0; i < size; i += 2) bus.write16(dst + i, buf[i] | ((i + 1 < size ? buf[i + 1] : 0) << 8));
        return true; }
    case 0x14: case 0x15: {                                                   // RLE
        uint32_t src = c.r[0], dst = c.r[1]; uint32_t hdr = bus.read32(src); src += 4; uint32_t size = hdr >> 8, out = 0; std::vector<uint8_t> buf;
        while (out < size) { uint32_t f = rd8(src++); if (f & 0x80) { uint32_t len = (f & 0x7F) + 3, v = rd8(src++); for (uint32_t k = 0; k < len && out < size; k++, out++) buf.push_back(v); }
            else { uint32_t len = (f & 0x7F) + 1; for (uint32_t k = 0; k < len && out < size; k++, out++) buf.push_back(rd8(src++)); } }
        if (n == 0x14) for (uint32_t i = 0; i < size; i++) bus.write8(dst + i, buf[i]);
        else for (uint32_t i = 0; i < size; i += 2) bus.write16(dst + i, buf[i] | ((i + 1 < size ? buf[i + 1] : 0) << 8));
        return true; }
    default: swi_unhandled[cpu][n]++; return true;                            // se ignora y se anota
    }
}

}  // namespace nds
