#include "layout.h"
#include <algorithm>

namespace nds {

static constexpr float kW = 256.f, kH = 192.f;

// Mayor escala que cabe en un cajon w x h conservando 256x192.
static float fit(float w, float h) { return std::max(0.f, std::min(w / kW, h / kH)); }

static Rect place(float cx, float top, float scale) {
    Rect r; r.w = kW * scale; r.h = kH * scale; r.x = cx - r.w / 2; r.y = top; r.visible = true; return r;
}

Layout compute_layout(float W, float H, int mode, bool swapped, float ratio) {
    Layout out;
    ratio = std::min(0.85f, std::max(0.5f, ratio));
    const bool land = W > H * 1.15f;                      // horizontal: las pantallas van una al lado de la otra
    float cx = W / 2;
    bool top_first = !swapped;                            // la pantalla superior de DS va arriba (vertical) o a la izquierda (horizontal)
    Rect* first = top_first ? &out.top : &out.bottom;     // posicion 1: arriba / izquierda
    Rect* second = top_first ? &out.bottom : &out.top;    // posicion 2: abajo / derecha

    auto single = [&](Rect* which) {
        *which = place(cx, 0, fit(W, H));
        which->y = (H - which->h) / 2;
    };
    if (mode == LAYOUT_SINGLE_TOP) { single(&out.top); return out; }
    if (mode == LAYOUT_SINGLE_BOTTOM) { single(&out.bottom); return out; }

    bool big = (mode == LAYOUT_BIG_TOP || mode == LAYOUT_BIG_BOTTOM);
    bool top_big = (mode == LAYOUT_BIG_TOP);
    bool first_big = big && (top_big == top_first);
    if (!land) {
        float h1 = !big ? H / 2 : (first_big ? H * ratio : H * (1 - ratio)), h2 = H - h1;
        Rect r1 = place(cx, 0, fit(W, h1)), r2 = place(cx, 0, fit(W, h2));
        float y0 = (H - (r1.h + r2.h)) / 2; r1.y = y0; r2.y = y0 + r1.h;
        *first = r1; *second = r2;
    } else {
        float w1 = !big ? W / 2 : (first_big ? W * ratio : W * (1 - ratio)), w2 = W - w1;
        Rect r1 = place(0, 0, fit(w1, H)), r2 = place(0, 0, fit(w2, H));
        float x0 = (W - (r1.w + r2.w)) / 2;
        r1.x = x0; r2.x = x0 + r1.w; r1.y = (H - r1.h) / 2; r2.y = (H - r2.h) / 2;
        *first = r1; *second = r2;
    }
    return out;
}

bool map_touch(const Layout& l, float x, float y, int* ds_x, int* ds_y) {
    const Rect& r = l.bottom;
    if (!r.visible || r.w <= 0 || r.h <= 0) return false;
    if (x < r.x || y < r.y || x >= r.x + r.w || y >= r.y + r.h) return false;
    int px = (int)((x - r.x) / r.w * kW), py = (int)((y - r.y) / r.h * kH);
    *ds_x = std::min(255, std::max(0, px));
    *ds_y = std::min(191, std::max(0, py));
    return true;
}

}  // namespace nds
