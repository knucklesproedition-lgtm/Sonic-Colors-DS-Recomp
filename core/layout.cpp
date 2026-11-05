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
    float cx = W / 2;
    Rect* up;    // pantalla que se coloca arriba
    Rect* down;  // pantalla que se coloca abajo
    // "up/down" son posiciones; top/bottom son pantallas de DS
    if (!swapped) { up = &out.top; down = &out.bottom; } else { up = &out.bottom; down = &out.top; }
    bool top_is_up = !swapped;

    auto single = [&](Rect* which) {
        *which = place(cx, 0, fit(W, H));
        which->y = (H - which->h) / 2;
    };

    switch (mode) {
    case LAYOUT_SINGLE_TOP:    single(&out.top);    break;
    case LAYOUT_SINGLE_BOTTOM: single(&out.bottom); break;
    case LAYOUT_BIG_TOP:
    case LAYOUT_BIG_BOTTOM: {
        bool top_big = (mode == LAYOUT_BIG_TOP);
        // Altura que se reserva a cada posicion (arriba/abajo)
        bool up_big = (top_big == top_is_up);
        float hu = up_big ? H * ratio : H * (1 - ratio);
        float hd = H - hu;
        float su = fit(W, hu), sd = fit(W, hd);
        Rect ru = place(cx, 0, su), rd = place(cx, 0, sd);
        float total = ru.h + rd.h, y0 = (H - total) / 2;
        ru.y = y0; rd.y = y0 + ru.h;
        *up = ru; *down = rd;
        break;
    }
    case LAYOUT_STACKED:
    default: {
        float s = fit(W, H / 2);
        Rect ru = place(cx, 0, s), rd = place(cx, 0, s);
        float y0 = (H - ru.h * 2) / 2;
        ru.y = y0; rd.y = y0 + ru.h;
        *up = ru; *down = rd;
        break;
    }
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
