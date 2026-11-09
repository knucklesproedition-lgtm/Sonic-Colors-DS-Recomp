// ds_video.cpp - Mapeo de bancos de VRAM, motor 2D (fondos de tiles + sprites) y composicion con el 3D.
// Soporta: fondos de tiles, afines, extendidos (mapa de 16 bits, bitmap 256 colores y color directo), sprites normales y afines,
// mezcla alfa / fundidos (BLDCNT), brillo maestro. NO implementado: ventanas, mosaico, paletas extendidas, captura de pantalla.
#include "ds_system.h"
#include <algorithm>
#include <cstring>

namespace nds {

void Console::rebuild_vram_map() {
    std::memset(map_bga_, 0, sizeof map_bga_); std::memset(map_obja_, 0, sizeof map_obja_); std::memset(map_bgb_, 0, sizeof map_bgb_);
    std::memset(map_objb_, 0, sizeof map_objb_); std::memset(map_lcdc_, 0, sizeof map_lcdc_); std::memset(map_tex_, 0, sizeof map_tex_);
    std::memset(map_texpal_, 0, sizeof map_texpal_);
    static const uint32_t size[9] = {0x20000, 0x20000, 0x20000, 0x20000, 0x10000, 0x4000, 0x4000, 0x8000, 0x4000};
    static const uint32_t off[9] = {0, 0x20000, 0x40000, 0x60000, 0x80000, 0x90000, 0x94000, 0x98000, 0xA0000};
    for (int b = 0; b < 9; b++) {
        uint8_t cnt = io[0][b < 7 ? 0x240 + b : 0x240 + b + 1];                // A-G: 0x240-0x246; H, I: 0x248, 0x249
        vramcnt_[b] = cnt;
        if (!(cnt & 0x80)) continue;
        int mst = cnt & 7, o = (cnt >> 3) & 3; uint8_t* base = vram + off[b]; int pages = size[b] >> 14;
        auto put = [&](uint8_t** arr, int len, int first) { for (int p = 0; p < pages; p++) if (first + p >= 0 && first + p < len) arr[first + p] = base + p * 0x4000; };
        int fg = (o & 1) + (o >> 1) * 4;                                       // posicion de los bancos pequenos F/G
        switch (b) {
        case 0: case 1: if (mst == 0) put(map_lcdc_, 41, off[b] >> 14); else if (mst == 1) put(map_bga_, 32, o * 8); else if (mst == 2) put(map_obja_, 16, (o & 1) * 8); else if (mst == 3) put(map_tex_, 32, o * 8); break;
        case 2: if (mst == 0) put(map_lcdc_, 41, off[b] >> 14); else if (mst == 1) put(map_bga_, 32, o * 8); else if (mst == 3) put(map_tex_, 32, o * 8); else if (mst == 4) put(map_bgb_, 8, 0); break;
        case 3: if (mst == 0) put(map_lcdc_, 41, off[b] >> 14); else if (mst == 1) put(map_bga_, 32, o * 8); else if (mst == 3) put(map_tex_, 32, o * 8); else if (mst == 4) put(map_objb_, 8, 0); break;
        case 4: if (mst == 0) put(map_lcdc_, 41, off[b] >> 14); else if (mst == 1) put(map_bga_, 32, 0); else if (mst == 2) put(map_obja_, 16, 0); else if (mst == 3) put(map_texpal_, 6, 0); break;
        case 5: case 6: if (mst == 0) put(map_lcdc_, 41, off[b] >> 14); else if (mst == 1) put(map_bga_, 32, fg); else if (mst == 2) put(map_obja_, 16, fg); else if (mst == 3) put(map_texpal_, 6, fg); break;
        case 7: if (mst == 0) put(map_lcdc_, 41, off[b] >> 14); else if (mst == 1) put(map_bgb_, 8, 0); break;
        default: if (mst == 0) put(map_lcdc_, 41, off[b] >> 14); else if (mst == 1) put(map_bgb_, 8, 2); else if (mst == 2) put(map_objb_, 8, 0); break;
        }
    }
    for (int i = 0; i < 32; i++) tex_cpages_[i] = map_tex_[i];
    for (int i = 0; i < 6; i++) pal_cpages_[i] = map_texpal_[i];
    gpu3d.set_mem(tex_cpages_, pal_cpages_);
}

static inline uint32_t abgr(uint32_t c555) {
    uint32_t r = c555 & 31, g = (c555 >> 5) & 31, b = (c555 >> 10) & 31;
    return 0xFF000000u | (((b << 3) | (b >> 2)) << 16) | (((g << 3) | (g >> 2)) << 8) | ((r << 3) | (r >> 2));
}

void Console::render_engine(int eng, std::vector<uint32_t>& out) {
    const int N = gfx_scale, W = 256 * N, H = 192 * N;
    out.assign((size_t)W * H, 0xFF000000u);
    auto io16 = [&](uint32_t o) -> uint32_t { return io[0][o] | (io[0][o + 1] << 8); };
    auto io32 = [&](uint32_t o) -> uint32_t { return io16(o) | (io16(o + 2) << 16); };
    uint32_t base = eng ? 0x1000 : 0, dispcnt = io32(base);
    int dmode = eng ? (dispcnt >> 16) & 1 : (dispcnt >> 16) & 3;
    auto fill = [&](uint32_t c) { std::fill(out.begin(), out.end(), c); };
    if ((dispcnt & 0x80) || dmode == 0) { fill(0xFFFFFFFFu); return; }
    if (dmode == 2 && !eng) {                                                    // visualizar un banco de VRAM tal cual
        const uint8_t* bank = vram + ((dispcnt >> 18) & 3) * 0x20000;
        for (int Y = 0; Y < H; Y++) for (int X = 0; X < W; X++) { uint32_t o = ((Y / N) * 256 + X / N) * 2; out[(size_t)Y * W + X] = abgr(bank[o] | (bank[o + 1] << 8)); }
        uint32_t br = io16(base + 0x6C); int bm2 = (br >> 14) & 3, bf2 = std::min<uint32_t>(16, br & 31);
        if (bm2 == 1 || bm2 == 2) for (auto& px : out) {
            uint32_t r = px & 255, g = (px >> 8) & 255, b = (px >> 16) & 255;
            if (bm2 == 1) { r += (255 - r) * bf2 / 16; g += (255 - g) * bf2 / 16; b += (255 - b) * bf2 / 16; } else { r -= r * bf2 / 16; g -= g * bf2 / 16; b -= b * bf2 / 16; }
            px = 0xFF000000u | (b << 16) | (g << 8) | r; }
        return;
    }
    if (dmode == 3) { return; }                                                  // FIFO de memoria principal: sin soporte (negro)

    auto pal16 = [&](uint32_t o) -> uint32_t { return pal[(eng * 0x400 + o) & 0x7FF] | (pal[((eng * 0x400 + o) & 0x7FF) + 1] << 8); };
    uint8_t** bgmap = eng ? map_bgb_ : map_bga_; int bgpages = eng ? 8 : 32;
    uint8_t** objmap = eng ? map_objb_ : map_obja_; int objpages = eng ? 8 : 16;
    auto rdp = [&](uint8_t** map, int pages, uint32_t a) -> uint32_t { uint32_t pi = a >> 14; if (pi >= (uint32_t)pages || !map[pi]) return 0; return map[pi][a & 0x3FFF]; };
    auto bg8 = [&](uint32_t a) { return rdp(bgmap, bgpages, a); };
    auto bg16 = [&](uint32_t a) { return bg8(a) | (bg8(a + 1) << 8); };

    const int mode = dispcnt & 7;
    bool bg0_3d = !eng && (dispcnt & 8) && (dispcnt & 0x100);
    // tipo de cada fondo segun el modo: 0 = tiles, 1 = afin (mapa de 8 bits), 2 = extendido (mapa de 16 bits o bitmap), 3 = no soportado
    static const int8_t kind[7][4] = {{0, 0, 0, 0}, {0, 0, 0, 1}, {0, 0, 1, 1}, {0, 0, 0, 2}, {0, 0, 1, 2}, {0, 0, 2, 2}, {0, 3, 3, 3}};
    static std::vector<int32_t> bgc[4]; int bgprio[4];
    for (int b = 0; b < 4; b++) {
        bgc[b].assign(256 * 192, -1);
        uint32_t cnt = io16(base + 8 + 2 * b); bgprio[b] = cnt & 3;
        if (!(dispcnt & (0x100 << b))) { bgprio[b] = 9; continue; }
        if (b == 0 && bg0_3d) continue;
        int k = kind[mode > 6 ? 6 : mode][b];
        if (k == 3) { unsupported_bg++; bgprio[b] = 9; continue; }
        uint32_t cb = ((cnt >> 2) & 15) * 0x4000 + (eng ? 0 : ((dispcnt >> 24) & 7) * 0x10000);
        uint32_t sb = ((cnt >> 8) & 31) * 0x800 + (eng ? 0 : ((dispcnt >> 27) & 7) * 0x10000);
        int sz = (cnt >> 14) & 3;
        if (k == 0) {                                                            // ---- fondo de tiles ----
            bool c256 = cnt & 0x80;
            uint32_t hof = io16(base + 0x10 + 4 * b) & 0x1FF, vof = io16(base + 0x12 + 4 * b) & 0x1FF;
            uint32_t wm = (sz & 1) ? 511 : 255, hm = (sz & 2) ? 511 : 255;
            for (int y = 0; y < 192; y++) for (int x = 0; x < 256; x++) {
                uint32_t px = (x + hof) & wm, py = (y + vof) & hm;
                uint32_t blk = (px >> 8) + (py >> 8) * ((sz & 1) ? 2 : 1);
                uint32_t ent = bg16(sb + blk * 0x800 + (((py & 255) >> 3) * 32 + ((px & 255) >> 3)) * 2);
                uint32_t tile = ent & 0x3FF, ix = px & 7, iy = py & 7; if (ent & 0x400) ix = 7 - ix; if (ent & 0x800) iy = 7 - iy;
                if (c256) { uint32_t idx = bg8(cb + tile * 64 + iy * 8 + ix); if (idx) bgc[b][y * 256 + x] = pal16(idx * 2); }
                else { uint32_t by = bg8(cb + tile * 32 + iy * 4 + (ix >> 1)); uint32_t idx = (ix & 1) ? by >> 4 : by & 15; if (idx) bgc[b][y * 256 + x] = pal16(((ent >> 12) * 16 + idx) * 2); }
            }
        } else {                                                                 // ---- fondo afin / extendido / bitmap (BG2 y BG3) ----
            uint32_t rb = base + (b == 2 ? 0x20 : 0x30);
            int32_t pa = (int16_t)io16(rb), pb = (int16_t)io16(rb + 2), pc = (int16_t)io16(rb + 4), pd = (int16_t)io16(rb + 6);
            int64_t X0 = (int32_t)(io32(rb + 8) << 4) >> 4, Y0 = (int32_t)(io32(rb + 12) << 4) >> 4;   // 28 bits con signo (20.8)
            bool wrap = cnt & 0x2000, bitmap = (k == 2) && (cnt & 4), direct = bitmap && (cnt & 0x80), tile16 = (k == 2) && !bitmap;
            int dimw, dimh;
            if (bitmap) { static const int bw[4] = {128, 256, 512, 512}, bh[4] = {128, 256, 256, 512}; dimw = bw[sz]; dimh = bh[sz]; }
            else dimw = dimh = 128 << sz;
            uint32_t bmbase = ((cnt >> 8) & 31) * 0x4000;
            for (int y = 0; y < 192; y++) {
                int64_t bx = X0 + (int64_t)y * pb, by = Y0 + (int64_t)y * pd;
                for (int x = 0; x < 256; x++) {
                    int64_t px = (bx + (int64_t)x * pa) >> 8, py = (by + (int64_t)x * pc) >> 8;
                    if (px < 0 || py < 0 || px >= dimw || py >= dimh) { if (!wrap) continue; px &= dimw - 1; py &= dimh - 1; }
                    if (direct) { uint32_t c = bg16(bmbase + (uint32_t)(py * dimw + px) * 2); if (c & 0x8000) bgc[b][y * 256 + x] = (int32_t)(c & 0x7FFF); }
                    else if (bitmap) { uint32_t idx = bg8(bmbase + (uint32_t)(py * dimw + px)); if (idx) bgc[b][y * 256 + x] = pal16(idx * 2); }
                    else {
                        uint32_t tw = dimw >> 3, ix = px & 7, iy = py & 7, tile;
                        if (tile16) { uint32_t ent = bg16(sb + (uint32_t)(((py >> 3) * tw + (px >> 3)) * 2)); tile = ent & 0x3FF; if (ent & 0x400) ix = 7 - ix; if (ent & 0x800) iy = 7 - iy; }
                        else tile = bg8(sb + (uint32_t)((py >> 3) * tw + (px >> 3)));
                        uint32_t idx = bg8(cb + tile * 64 + iy * 8 + ix); if (idx) bgc[b][y * 256 + x] = pal16(idx * 2);
                    }
                }
            }
        }
    }
    // ---- sprites (normales y afines) ----
    static std::vector<int32_t> objc; static std::vector<uint8_t> objp, objsemi; objc.assign(256 * 192, -1); objp.assign(256 * 192, 9); objsemi.assign(256 * 192, 0);
    if (dispcnt & 0x1000) {
        const uint8_t* oam_ = oam + eng * 0x400; bool one_d = dispcnt & 0x10; uint32_t bnd = (dispcnt >> 20) & 3;
        static const int dims[3][4][2] = {{{8, 8}, {16, 16}, {32, 32}, {64, 64}}, {{16, 8}, {32, 8}, {32, 16}, {64, 32}}, {{8, 16}, {8, 32}, {16, 32}, {32, 64}}};
        auto oam16 = [&](int off) -> int32_t { return (int16_t)(oam_[off] | (oam_[off + 1] << 8)); };
        for (int i = 0; i < 128; i++) {
            uint32_t a0 = oam_[i * 8] | (oam_[i * 8 + 1] << 8), a1 = oam_[i * 8 + 2] | (oam_[i * 8 + 3] << 8), a2 = oam_[i * 8 + 4] | (oam_[i * 8 + 5] << 8);
            bool affine = a0 & 0x100, dbl = affine && (a0 & 0x200);
            if (!affine && (a0 & 0x200)) continue;                               // desactivado
            uint32_t omode = (a0 >> 10) & 3; if (omode == 2 || omode == 3) continue;   // ventana / bitmap: sin soporte
            uint32_t shape = a0 >> 14; if (shape == 3) continue;
            int w = dims[shape][a1 >> 14][0], h = dims[shape][a1 >> 14][1];
            int bw = dbl ? w * 2 : w, bh = dbl ? h * 2 : h;                      // caja que ocupa en pantalla
            int sy = a0 & 0xFF, sx = a1 & 0x1FF; if (sx >= 256) sx -= 512;
            bool c256 = a0 & 0x2000, hf = !affine && (a1 & 0x1000), vf = !affine && (a1 & 0x2000);
            uint32_t tileno = a2 & 0x3FF, prio = (a2 >> 10) & 3, pb_ = a2 >> 12;
            int32_t pa = 0x100, pbm = 0, pc = 0, pd = 0x100;
            if (affine) { int g = (a1 >> 9) & 31; pa = oam16(g * 32 + 6); pbm = oam16(g * 32 + 14); pc = oam16(g * 32 + 22); pd = oam16(g * 32 + 30); }
            for (int ly = 0; ly < bh; ly++) {
                int y = (sy + ly) & 0xFF; if (y >= 192) continue;
                for (int lx = 0; lx < bw; lx++) {
                    int x = sx + lx; if (x < 0 || x >= 256) continue;
                    int tx, ty;
                    if (affine) { int dx = lx - bw / 2, dy = ly - bh / 2; tx = ((pa * dx + pbm * dy) >> 8) + w / 2; ty = ((pc * dx + pd * dy) >> 8) + h / 2; if (tx < 0 || ty < 0 || tx >= w || ty >= h) continue; }
                    else { tx = hf ? w - 1 - lx : lx; ty = vf ? h - 1 - ly : ly; }
                    int ttx = tx >> 3, tty = ty >> 3, ix = tx & 7, iy = ty & 7;
                    uint32_t addr;
                    if (one_d) addr = (tileno << (5 + bnd)) + (tty * (w >> 3) + ttx) * (c256 ? 64 : 32);
                    else addr = (tileno + tty * 32 + ttx * (c256 ? 2 : 1)) * 32;
                    uint32_t idx;
                    if (c256) idx = rdp(objmap, objpages, addr + iy * 8 + ix);
                    else { uint32_t by = rdp(objmap, objpages, addr + iy * 4 + (ix >> 1)); idx = (ix & 1) ? by >> 4 : by & 15; }
                    if (!idx) continue;
                    size_t p = (size_t)y * 256 + x;
                    if (prio < objp[p]) { objp[p] = (uint8_t)prio; objc[p] = pal16(0x200 + (c256 ? idx : pb_ * 16 + idx) * 2); objsemi[p] = (omode == 1); }
                }
            }
        }
    }
    // ---- composicion: dos capas mas altas por pixel + mezcla alfa / fundidos (BLDCNT) ----
    uint32_t backdrop = pal16(0);
    uint32_t bld = io16(base + 0x50), eff = (bld >> 6) & 3, tgt1 = bld & 0x3F, tgt2 = (bld >> 8) & 0x3F;
    uint32_t balpha = io16(base + 0x52), eva = std::min<uint32_t>(16, balpha & 31), evb = std::min<uint32_t>(16, (balpha >> 8) & 31), evy = std::min<uint32_t>(16, io16(base + 0x54) & 31);
    static std::vector<int32_t> col; static std::vector<uint8_t> wprio, wobj; col.resize(256 * 192); wprio.resize(256 * 192); wobj.resize(256 * 192);
    auto mix = [](uint32_t c1, uint32_t c2, uint32_t a, uint32_t b) { uint32_t o = 0; for (int s = 0; s < 15; s += 5) o |= std::min<uint32_t>(31, (((c1 >> s) & 31) * a + ((c2 >> s) & 31) * b) / 16) << s; return o; };
    for (int p = 0; p < 256 * 192; p++) {
        int ln[2] = {5, 5}; int32_t cc[2] = {(int32_t)backdrop, (int32_t)backdrop}; uint8_t wp = 4, wo = 0; int found = 0;
        for (int pr = 0; pr < 4 && found < 2; pr++) {
            if (objp[p] == pr && objc[p] >= 0) { if (found == 0) { wp = (uint8_t)pr; wo = 1; } ln[found] = 4; cc[found] = objc[p]; found++; if (found == 2) break; }
            for (int b = 0; b < 4 && found < 2; b++) if (bgprio[b] == pr && bgc[b][p] >= 0) { if (found == 0) wp = (uint8_t)pr; ln[found] = b; cc[found] = bgc[b][p]; found++; }
        }
        uint32_t c = (uint32_t)cc[0];
        bool semi = (ln[0] == 4) && objsemi[p];
        if ((semi || (eff == 1 && (tgt1 & (1u << ln[0])))) && (tgt2 & (1u << ln[1])) && ln[0] != ln[1]) c = mix(cc[0], cc[1], eva, evb);
        else if (eff == 2 && (tgt1 & (1u << ln[0]))) { uint32_t o = 0; for (int s = 0; s < 15; s += 5) { uint32_t ch = (c >> s) & 31; o |= (ch + (31 - ch) * evy / 16) << s; } c = o; }
        else if (eff == 3 && (tgt1 & (1u << ln[0]))) { uint32_t o = 0; for (int s = 0; s < 15; s += 5) { uint32_t ch = (c >> s) & 31; o |= (ch - ch * evy / 16) << s; } c = o; }
        col[p] = (int32_t)c; wprio[p] = wp; wobj[p] = wo;
    }
    // ---- salida a resolucion interna, con el 3D como fondo 0 ----
    const std::vector<uint32_t>* f3 = nullptr; int p3 = 9;
    if (bg0_3d && gpu3d.scale() == N) { f3 = &gpu3d.frame(); p3 = io16(base + 8) & 3; }
    uint32_t bright = io16(base + 0x6C); int bm = (bright >> 14) & 3, bf = std::min<uint32_t>(16, bright & 31);
    for (int Y = 0; Y < H; Y++) for (int X = 0; X < W; X++) {
        int p = (Y / N) * 256 + (X / N); uint32_t px = abgr((uint32_t)col[p]);
        if (f3) { uint32_t s = (*f3)[(size_t)Y * W + X];
            if ((s >> 24) && (p3 < wprio[p] || (p3 == wprio[p] && !wobj[p]))) px = 0xFF000000u | ((s & 0xFF) << 16) | (s & 0xFF00) | ((s >> 16) & 0xFF); }
        if (bm == 1 || bm == 2) {
            uint32_t r = px & 255, g = (px >> 8) & 255, b = (px >> 16) & 255;
            if (bm == 1) { r += (255 - r) * bf / 16; g += (255 - g) * bf / 16; b += (255 - b) * bf / 16; } else { r -= r * bf / 16; g -= g * bf / 16; b -= b * bf / 16; }
            px = 0xFF000000u | (b << 16) | (g << 8) | r;
        }
        out[(size_t)Y * W + X] = px;
    }
}

void Console::render_frame() {
    if (gpu3d.scale() != gfx_scale) gpu3d.set_scale(gfx_scale);
    std::vector<uint32_t> a, b; render_engine(0, a); render_engine(1, b);
    bool a_top = io[0][0x305] & 0x80;                                           // POWCNT1 bit 15: motor A en la pantalla superior
    if (a_top) { fb[0].swap(a); fb[1].swap(b); } else { fb[0].swap(b); fb[1].swap(a); }
    video_frames++;
}

}  // namespace nds
