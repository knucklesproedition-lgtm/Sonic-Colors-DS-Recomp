#include "gpu3d.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace nds {

static inline int32_t sext(uint32_t v, int bits) { return (int32_t)(v << (32 - bits)) >> (32 - bits); }
static inline float fx12(uint32_t v) { return (int32_t)v / 4096.0f; }

Gpu3D::Gpu3D() { for (auto& m : pos_stack_) m = identity(); set_scale(1); }

void Gpu3D::set_scale(int n) {
    scale_ = std::max(1, std::min(4, n));
    color_.assign((size_t)width() * height(), 0); depth_.assign((size_t)width() * height(), 1e30f);
}

Gpu3D::Mat Gpu3D::identity() { Mat r; std::memset(r.m, 0, sizeof r.m); r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1; return r; }
Gpu3D::Mat Gpu3D::mul(const Mat& a, const Mat& b) {          // a * b
    Mat r;
    for (int c = 0; c < 4; c++) for (int rw = 0; rw < 4; rw++) {
        float s = 0; for (int k = 0; k < 4; k++) s += a.m[k * 4 + rw] * b.m[c * 4 + k]; r.m[c * 4 + rw] = s; }
    return r;
}
Gpu3D::Mat& Gpu3D::cur() { return mode_ == 0 ? proj_ : mode_ == 3 ? tex_m_ : pos_; }

int Gpu3D::param_count(uint32_t c) {
    switch (c) {
    case 0x10: case 0x12: case 0x13: case 0x14: return 1;
    case 0x16: case 0x18: return 16; case 0x17: case 0x19: return 12; case 0x1A: return 9; case 0x1B: case 0x1C: return 3;
    case 0x20: case 0x21: case 0x22: case 0x24: case 0x25: case 0x26: case 0x27: case 0x28: case 0x29: case 0x2A: case 0x2B: return 1;
    case 0x23: return 2;
    case 0x30: case 0x31: case 0x32: case 0x33: return 1; case 0x34: return 32;
    case 0x40: case 0x50: case 0x60: return 1; case 0x70: return 3; case 0x71: return 2; case 0x72: return 1;
    default: return 0;                                         // 0x11, 0x15, 0x41 y desconocidos: sin parametros
    }
}

void Gpu3D::advance() {
    while (cmd_idx_ < 4) {
        uint32_t c = (packed_ >> (8 * cmd_idx_)) & 0xFF; cmd_idx_++;
        if (c == 0) continue;
        int n = param_count(c);
        if (n == 0) { exec(c, nullptr); continue; }
        cur_cmd_ = c; need_ = n; have_ = 0; return;
    }
}
void Gpu3D::fifo_write(uint32_t v) {
    if (need_ > 0) { params_[have_++] = v; if (have_ == need_) { need_ = 0; exec(cur_cmd_, params_); advance(); } return; }
    packed_ = v; cmd_idx_ = 0; advance();
}
void Gpu3D::port_write(uint32_t cmd, uint32_t v) {
    cmd &= 0xFF; int n = param_count(cmd);
    if (n == 0) { exec(cmd, nullptr); return; }
    port_params_[cmd][port_have_[cmd]++] = v;
    if (port_have_[cmd] >= n) { port_have_[cmd] = 0; exec(cmd, port_params_[cmd]); }
}

void Gpu3D::exec(uint32_t cmd, const uint32_t* p) {
    switch (cmd) {
    case 0x10: mode_ = p[0] & 3; break;
    case 0x11: if (mode_ == 0) proj_stack_ = proj_; else if (sp_ < 31) pos_stack_[sp_++] = pos_; break;
    case 0x12: { int n = sext(p[0] & 0x3F, 6); if (mode_ == 0) proj_ = proj_stack_; else { sp_ = std::max(0, std::min(31, sp_ - n)); pos_ = pos_stack_[std::min(sp_, 31)]; } break; }
    case 0x13: pos_stack_[std::min<uint32_t>(p[0] & 0x1F, 31)] = pos_; break;
    case 0x14: pos_ = pos_stack_[std::min<uint32_t>(p[0] & 0x1F, 31)]; break;
    case 0x15: cur() = identity(); break;
    case 0x16: case 0x18: { Mat m; for (int i = 0; i < 16; i++) m.m[i] = fx12(p[i]); cur() = (cmd == 0x16) ? m : mul(cur(), m); break; }
    case 0x17: case 0x19: { Mat m = identity(); for (int c = 0; c < 4; c++) for (int r = 0; r < 3; r++) m.m[c * 4 + r] = fx12(p[c * 3 + r]); cur() = (cmd == 0x17) ? m : mul(cur(), m); break; }
    case 0x1A: { Mat m = identity(); for (int c = 0; c < 3; c++) for (int r = 0; r < 3; r++) m.m[c * 4 + r] = fx12(p[c * 3 + r]); cur() = mul(cur(), m); break; }
    case 0x1B: { Mat m = identity(); m.m[0] = fx12(p[0]); m.m[5] = fx12(p[1]); m.m[10] = fx12(p[2]); cur() = mul(cur(), m); break; }
    case 0x1C: { Mat m = identity(); m.m[12] = fx12(p[0]); m.m[13] = fx12(p[1]); m.m[14] = fx12(p[2]); cur() = mul(cur(), m); break; }
    case 0x20: cr_ = (p[0] & 31) / 31.0f; cg_ = ((p[0] >> 5) & 31) / 31.0f; cb_ = ((p[0] >> 10) & 31) / 31.0f; break;
    case 0x22: ts_ = (int16_t)(p[0] & 0xFFFF) / 16.0f; tt_ = (int16_t)(p[0] >> 16) / 16.0f; break;
    case 0x23: vx_ = fx12((uint32_t)(int16_t)(p[0] & 0xFFFF)); vy_ = fx12((uint32_t)(int16_t)(p[0] >> 16)); vz_ = fx12((uint32_t)(int16_t)(p[1] & 0xFFFF)); submit_vertex(); break;
    case 0x24: vx_ = sext(p[0] & 0x3FF, 10) / 64.0f; vy_ = sext((p[0] >> 10) & 0x3FF, 10) / 64.0f; vz_ = sext((p[0] >> 20) & 0x3FF, 10) / 64.0f; submit_vertex(); break;
    case 0x25: vx_ = fx12((uint32_t)(int16_t)(p[0] & 0xFFFF)); vy_ = fx12((uint32_t)(int16_t)(p[0] >> 16)); submit_vertex(); break;
    case 0x26: vx_ = fx12((uint32_t)(int16_t)(p[0] & 0xFFFF)); vz_ = fx12((uint32_t)(int16_t)(p[0] >> 16)); submit_vertex(); break;
    case 0x27: vy_ = fx12((uint32_t)(int16_t)(p[0] & 0xFFFF)); vz_ = fx12((uint32_t)(int16_t)(p[0] >> 16)); submit_vertex(); break;
    case 0x28: vx_ += sext(p[0] & 0x3FF, 10) / 512.0f; vy_ += sext((p[0] >> 10) & 0x3FF, 10) / 512.0f; vz_ += sext((p[0] >> 20) & 0x3FF, 10) / 512.0f; submit_vertex(); break;
    case 0x29: attr_ = p[0]; break;
    case 0x2A: tex_param_ = p[0]; break;
    case 0x2B: pltt_base_ = p[0] & 0x1FFF; break;
    case 0x40: prim_ = p[0] & 3; attr_latched_ = attr_; assem_n_ = 0; strip_flip_ = false; strip_count_ = 0; break;
    case 0x41: break;
    case 0x50: swap_buffers(); break;
    case 0x60: vp_[0] = p[0] & 0xFF; vp_[1] = (p[0] >> 8) & 0xFF; vp_[2] = (p[0] >> 16) & 0xFF; vp_[3] = (p[0] >> 24) & 0xFF; break;
    case 0x21: case 0x30: case 0x31: case 0x32: case 0x33: case 0x34: case 0x70: case 0x71: case 0x72: break;   // iluminacion/tests: aun sin soporte
    default: unknown_cmds++; last_unknown_cmd = cmd; break;
    }
}

void Gpu3D::submit_vertex() {
    // Posicion -> espacio de recorte: proj * pos * (x, y, z, 1)
    Mat mvp = mul(proj_, pos_);
    Vtx v;
    v.x = mvp.m[0] * vx_ + mvp.m[4] * vy_ + mvp.m[8] * vz_ + mvp.m[12];
    v.y = mvp.m[1] * vx_ + mvp.m[5] * vy_ + mvp.m[9] * vz_ + mvp.m[13];
    v.z = mvp.m[2] * vx_ + mvp.m[6] * vy_ + mvp.m[10] * vz_ + mvp.m[14];
    v.w = mvp.m[3] * vx_ + mvp.m[7] * vy_ + mvp.m[11] * vz_ + mvp.m[15];
    v.r = cr_; v.g = cg_; v.b = cb_; v.s = ts_; v.t = tt_;
    switch (prim_) {
    case 0: assem_[assem_n_++] = v; if (assem_n_ == 3) { finish_polygon(assem_, 3); assem_n_ = 0; } break;
    case 1: assem_[assem_n_++] = v; if (assem_n_ == 4) { finish_polygon(assem_, 4); assem_n_ = 0; } break;
    case 2: {                                                   // tira de triangulos
        assem_[assem_n_++ % 3] = v; strip_count_++;
        if (strip_count_ >= 3) {
            Vtx t[3]; int i0 = (strip_count_ - 3) % 3, i1 = (strip_count_ - 2) % 3, i2 = (strip_count_ - 1) % 3;
            if (strip_count_ % 2 == 1) { t[0] = assem_[i0]; t[1] = assem_[i1]; t[2] = assem_[i2]; } else { t[0] = assem_[i1]; t[1] = assem_[i0]; t[2] = assem_[i2]; }
            finish_polygon(t, 3);
        }
        break; }
    default: {                                                  // tira de cuadrilateros: v0 v1 v3 v2
        assem_[assem_n_++] = v;
        if (assem_n_ == 4) { Vtx q[4] = {assem_[0], assem_[1], assem_[3], assem_[2]}; finish_polygon(q, 4); assem_[0] = assem_[2]; assem_[1] = assem_[3]; assem_n_ = 2; }
        break; }
    }
}

// Recorte contra los planos cercano y lejano (suficiente: x/y se recortan al rasterizar).
void Gpu3D::finish_polygon(const Vtx* in, int n) {
    Vtx a[10], b[10]; int na = n; for (int i = 0; i < n; i++) a[i] = in[i];
    auto lerp = [](const Vtx& p, const Vtx& q, float t) { Vtx r; const float* pp = &p.x; const float* qq = &q.x; float* rr = &r.x; for (int i = 0; i < 9; i++) rr[i] = pp[i] + (qq[i] - pp[i]) * t; return r; };
    for (int plane = 0; plane < 2; plane++) {
        int nb = 0;
        auto dist = [&](const Vtx& v) { return plane == 0 ? v.z + v.w : v.w - v.z; };      // >= 0 dentro
        for (int i = 0; i < na; i++) {
            const Vtx& p = a[i]; const Vtx& q = a[(i + 1) % na]; float dp = dist(p), dq = dist(q);
            if (dp >= 0) b[nb++] = p;
            if ((dp >= 0) != (dq >= 0) && nb < 10) b[nb++] = lerp(p, q, dp / (dp - dq));
        }
        na = nb; for (int i = 0; i < na; i++) a[i] = b[i];
        if (na < 3) return;
    }
    Poly poly; poly.n = 0; poly.attr = attr_latched_; poly.tex = tex_param_; poly.pltt = pltt_base_;
    for (int i = 0; i < na; i++) { if (a[i].w <= 1e-6f) return; poly.v[poly.n++] = a[i]; }
    // cara: area en NDC (y arriba). CCW = frontal. Bits 6/7 de POLYGON_ATTR eligen que caras se dibujan.
    float area = 0;
    for (int i = 0; i < poly.n; i++) { const Vtx& p = poly.v[i]; const Vtx& q = poly.v[(i + 1) % poly.n]; area += (p.x / p.w) * (q.y / q.w) - (q.x / q.w) * (p.y / p.w); }
    bool front = area > 0;
    bool draw_back = poly.attr & 0x40, draw_front = poly.attr & 0x80;
    if (area == 0 || (front && !draw_front) || (!front && !draw_back)) { polys_culled++; return; }
    polys_.push_back(poly);
}

uint32_t Gpu3D::texel(const Poly& p, int x, int y) const {
    uint32_t fmt = (p.tex >> 26) & 7, ow = 8u << ((p.tex >> 20) & 7);
    uint32_t base = (p.tex & 0xFFFF) * 8; bool c0t = p.tex & (1u << 29);
    auto rd8 = [&](uint32_t a) -> uint32_t { return tex_ && tex_[(a >> 14) & 31] ? tex_[(a >> 14) & 31][a & 0x3FFF] : 0; };
    auto rd16 = [&](uint32_t a) -> uint32_t { return rd8(a) | (rd8(a + 1) << 8); };
    auto pal = [&](uint32_t a) -> uint32_t { a &= 0x17FFF; return pal_ && pal_[(a >> 14) % 6] ? (uint32_t)(pal_[(a >> 14) % 6][a & 0x3FFF] | (pal_[(a >> 14) % 6][(a & 0x3FFF) + 1] << 8)) : 0; };
    auto argb = [](uint32_t c555, uint32_t a8) { uint32_t r = c555 & 31, g = (c555 >> 5) & 31, b = (c555 >> 10) & 31; return (a8 << 24) | (((r << 3) | (r >> 2)) << 16) | (((g << 3) | (g >> 2)) << 8) | ((b << 3) | (b >> 2)); };
    uint32_t pb = p.pltt * 16, idx, a;
    switch (fmt) {
    case 1: { idx = rd8(base + y * ow + x); a = idx >> 5; uint32_t a5 = (a << 2) | (a >> 1); return argb(pal(pb + (idx & 31) * 2), a5 * 255 / 31); }   // A3I5: alfa de 3 bits -> 5 bits -> 8 bits
    case 2: { idx = (rd8(base + (y * ow + x) / 4) >> (2 * ((y * ow + x) & 3))) & 3; if (idx == 0 && c0t) return 0; return argb(pal(p.pltt * 8 + idx * 2), 255); }
    case 3: { idx = (rd8(base + (y * ow + x) / 2) >> (4 * ((y * ow + x) & 1))) & 15; if (idx == 0 && c0t) return 0; return argb(pal(pb + idx * 2), 255); }
    case 4: idx = rd8(base + y * ow + x); if (idx == 0 && c0t) return 0; return argb(pal(pb + idx * 2), 255);
    case 5: {                                                   // comprimida 4x4
        uint32_t bx = x / 4, by = y / 4, blk = by * (ow / 4) + bx;
        uint32_t row = rd8(base + blk * 4 + (y & 3)); uint32_t t = (row >> (2 * (x & 3))) & 3;
        uint32_t info = rd16(((base >> 17) + 1) * 0x20000 + ((base & 0x1FFFF) >> 1) + blk * 2), mode = info >> 14, po = (info & 0x3FFF) * 4 + p.pltt * 16;
        uint32_t c0 = pal(po), c1 = pal(po + 2);
        auto mix = [&](uint32_t wa, uint32_t wb, uint32_t den) { uint32_t o = 0; for (int s = 0; s < 15; s += 5) o |= ((((c0 >> s) & 31) * wa + ((c1 >> s) & 31) * wb) / den) << s; return o; };
        switch (t) { case 0: return argb(c0, 255); case 1: return argb(c1, 255);
            case 2: return mode == 0 ? argb(pal(po + 4), 255) : mode == 1 ? argb(mix(1, 1, 2), 255) : mode == 2 ? argb(pal(po + 4), 255) : argb(mix(5, 3, 8), 255);
            default: return mode == 2 ? argb(pal(po + 6), 255) : mode == 3 ? argb(mix(3, 5, 8), 255) : 0; }
    }
    case 6: idx = rd8(base + y * ow + x); a = idx >> 3; return argb(pal(pb + (idx & 7) * 2), a * 255 / 31);
    case 7: { uint32_t c = rd16(base + (y * ow + x) * 2); return (c & 0x8000) ? argb(c, 255) : 0; }
    default: return argb(0x7C1F, 255);
    }
}

void Gpu3D::draw_triangle(const Poly& p, const Vtx& A, const Vtx& B, const Vtx& C) {
    const int W = width(), H = height(); const float S = (float)scale_;
    const Vtx* v[3] = {&A, &B, &C}; float sx[3], sy[3], sz[3], iw[3];
    for (int i = 0; i < 3; i++) {
        float nx = v[i]->x / v[i]->w, ny = v[i]->y / v[i]->w; iw[i] = 1.0f / v[i]->w; sz[i] = v[i]->z / v[i]->w;
        float px = vp_[0] + (nx * 0.5f + 0.5f) * (vp_[2] - vp_[0] + 1), py = 191.0f - (vp_[1] + (ny * 0.5f + 0.5f) * (vp_[3] - vp_[1] + 1));
        sx[i] = px * S; sy[i] = py * S;
    }
    float area = (sx[1] - sx[0]) * (sy[2] - sy[0]) - (sx[2] - sx[0]) * (sy[1] - sy[0]);
    if (std::fabs(area) < 1e-9f) return;
    int x0 = std::max(0, (int)std::floor(std::min({sx[0], sx[1], sx[2]}))), x1 = std::min(W - 1, (int)std::ceil(std::max({sx[0], sx[1], sx[2]})));
    int y0 = std::max(0, (int)std::floor(std::min({sy[0], sy[1], sy[2]}))), y1 = std::min(H - 1, (int)std::ceil(std::max({sy[0], sy[1], sy[2]})));
    uint32_t alpha5 = (p.attr >> 16) & 31; if (alpha5 == 0) return;                 // alfa 0 = alambre: sin soporte
    float pa = alpha5 / 31.0f; bool textured = ((p.tex >> 26) & 7) != 0; (void)textured;
    uint32_t fmt = (p.tex >> 26) & 7; bool use_tex = fmt != 0 && tex_ != nullptr;
    uint32_t tw = 8u << ((p.tex >> 20) & 7), th = 8u << ((p.tex >> 23) & 7);
    bool rep_s = p.tex & (1u << 16), rep_t = p.tex & (1u << 17), flip_s = p.tex & (1u << 18), flip_t = p.tex & (1u << 19);
    bool depth_eq = p.attr & (1u << 14), depth_upd_trans = p.attr & (1u << 11); uint32_t pmode = (p.attr >> 4) & 3;
    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
        float cx = x + 0.5f, cy = y + 0.5f;
        float w0 = ((sx[1] - cx) * (sy[2] - cy) - (sx[2] - cx) * (sy[1] - cy)) / area;
        float w1 = ((sx[2] - cx) * (sy[0] - cy) - (sx[0] - cx) * (sy[2] - cy)) / area;
        float w2 = 1.0f - w0 - w1;
        if (w0 < -1e-6f || w1 < -1e-6f || w2 < -1e-6f) continue;
        float z = w0 * sz[0] + w1 * sz[1] + w2 * sz[2];
        size_t idx = (size_t)y * W + x; float dz = depth_[idx];
        if (depth_eq ? std::fabs(z - dz) > 0.002f : !(z < dz)) continue;
        float d = w0 * iw[0] + w1 * iw[1] + w2 * iw[2]; float l0 = w0 * iw[0] / d, l1 = w1 * iw[1] / d, l2 = w2 * iw[2] / d;
        float r = l0 * v[0]->r + l1 * v[1]->r + l2 * v[2]->r, g = l0 * v[0]->g + l1 * v[1]->g + l2 * v[2]->g, b = l0 * v[0]->b + l1 * v[1]->b + l2 * v[2]->b;
        float a = pa;
        if (use_tex) {
            float s = l0 * v[0]->s + l1 * v[1]->s + l2 * v[2]->s, t = l0 * v[0]->t + l1 * v[1]->t + l2 * v[2]->t;
            int ts = (int)std::floor(s), tt = (int)std::floor(t);
            auto wrap = [](int c, int size, bool rep, bool flip) { if (rep) { int q = c / size; if (c < 0 && c % size) q--; int m = c - q * size; return (flip && (q & 1)) ? size - 1 - m : m; } return std::max(0, std::min(size - 1, c)); };
            uint32_t tx = texel(p, wrap(ts, (int)tw, rep_s, flip_s), wrap(tt, (int)th, rep_t, flip_t));
            float ta = (tx >> 24) / 255.0f; if (ta <= 0.0f) continue;
            float tr = ((tx >> 16) & 255) / 255.0f, tg = ((tx >> 8) & 255) / 255.0f, tb = (tx & 255) / 255.0f;
            if (pmode == 1) { r = tr * ta + r * (1 - ta); g = tg * ta + g * (1 - ta); b = tb * ta + b * (1 - ta); }   // calco
            else { r *= tr; g *= tg; b *= tb; }                                                                          // modulacion
            a *= ta;
        }
        auto to8 = [](float c) { return (uint32_t)std::max(0.0f, std::min(255.0f, c * 255.0f + 0.5f)); };
        uint32_t src = (to8(r) << 16) | (to8(g) << 8) | to8(b);
        if (a >= 0.999f) { color_[idx] = 0xFF000000u | src; depth_[idx] = z; }
        else {
            uint32_t dst = color_[idx]; float da = (dst >> 24) / 255.0f;
            auto mix = [&](int sh) { float s = ((src >> sh) & 255) * a, d2 = ((dst >> sh) & 255) * (1 - a) * (da > 0 ? 1.f : 0.f); return (uint32_t)std::min(255.0f, (s + d2) / (a + (1 - a) * (da > 0 ? 1.f : 0.f)) + 0.5f); };
            uint32_t oa = to8(std::max(da, a));
            color_[idx] = (oa << 24) | (mix(16) << 16) | (mix(8) << 8) | mix(0);
            if (depth_upd_trans) depth_[idx] = z;
        }
    }
}

void Gpu3D::rasterize(const Poly& p) {
    for (int i = 1; i + 1 < p.n; i++) draw_triangle(p, p.v[0], p.v[i], p.v[i + 1]);
    polys_drawn++; if (((p.tex >> 26) & 7) != 0) textured_polys++;
}

void Gpu3D::swap_buffers() {
    uint32_t cc = clear_color_, ca = (cc >> 16) & 31;
    uint32_t r = cc & 31, g = (cc >> 5) & 31, b = (cc >> 10) & 31;
    uint32_t fill = ca ? (0xFF000000u | (((r << 3) | (r >> 2)) << 16) | (((g << 3) | (g >> 2)) << 8) | ((b << 3) | (b >> 2))) : 0u;
    std::fill(color_.begin(), color_.end(), fill); std::fill(depth_.begin(), depth_.end(), 1e30f);
    for (const Poly& p : polys_) if (((p.attr >> 16) & 31) == 31) rasterize(p);        // primero opacos
    for (const Poly& p : polys_) if (((p.attr >> 16) & 31) != 31) rasterize(p);        // luego translucidos
    polys_.clear(); frames++;
    sp_ = 0;
}

}  // namespace nds
