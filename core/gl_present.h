// gl_present.h - Presenta las dos pantallas de DS con OpenGL ES 3: reescalado y filtrado.
// Filtros: 0 = pixeles (nearest), 1 = suave (bilinear), 2 = nitido (sharp bilinear: texels definidos, bordes suavizados).
#pragma once
#include <cstdint>
#include "layout.h"

namespace nds {

class GlPresenter {
public:
    bool init();                       // crea shader y texturas (hay que llamarlo en el hilo de GL, tras crear el contexto)
    void shutdown();
    // top/bottom: RGBA (byte 0 = R) de w x h pixeles. 'layout' esta en pixeles de la vista.
    void draw(const uint32_t* top, const uint32_t* bottom, int w, int h, const Layout& layout, int view_w, int view_h, int filter);
private:
    unsigned prog_ = 0, vbo_ = 0, vao_ = 0, tex_[2] = {0, 0};
    int loc_rect_ = -1, loc_view_ = -1, loc_size_ = -1, loc_scale_ = -1, loc_mode_ = -1, loc_tex_ = -1;
    void draw_one(int idx, const uint32_t* px, int w, int h, const Rect& r, int vw, int vh, int filter);
    bool ready_ = false;
};

}  // namespace nds
