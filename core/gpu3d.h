// gpu3d.h - Motor 3D de la DS: cola de comandos, matrices, ensamblado de poligonos, recorte y rasterizado
// por software a resolucion interna 1x..4x. (OpenGL solo se usa luego para reescalar/filtrar la imagen.)
// NO implementado todavia: iluminacion, toon/highlight/sombras, niebla, antialiasing, contornos, matriz de textura.
#pragma once
#include <cstdint>
#include <vector>

namespace nds {

class Gpu3D {
public:
    Gpu3D();
    // Memoria de texturas: 32 paginas de 16 KB (4 ranuras x 128 KB) y 6 paginas de paleta (96 KB).
    void set_mem(const uint8_t* const* tex_pages, const uint8_t* const* pal_pages) { tex_ = tex_pages; pal_ = pal_pages; }
    void set_scale(int n);
    int scale() const { return scale_; }
    int width() const { return 256 * scale_; }
    int height() const { return 192 * scale_; }

    void fifo_write(uint32_t v);                         // 0x04000400 (comandos empaquetados)
    void port_write(uint32_t cmd, uint32_t v);           // puertos directos 0x04000440..: cmd = (direccion - 0x400) / 4
    void set_clear_color(uint32_t v) { clear_color_ = v; }
    void set_clear_depth(uint32_t v) { clear_depth_ = v; }

    // Imagen ARGB del ultimo SWAP_BUFFERS (A = 0: transparente). Tamano width() x height().
    const std::vector<uint32_t>& frame() const { return color_; }
    uint64_t frames = 0, polys_drawn = 0, polys_culled = 0, unknown_cmds = 0, textured_polys = 0;
    uint32_t last_unknown_cmd = 0;

private:
    struct Mat { float m[16]; };                          // columna-mayor, vector columna (como OpenGL)
    struct Vtx { float x, y, z, w, r, g, b, s, t; };
    struct Poly { Vtx v[10]; int n = 0; uint32_t attr = 0, tex = 0, pltt = 0; };

    static Mat identity();
    static Mat mul(const Mat& a, const Mat& b);
    Mat& cur();
    void exec(uint32_t cmd, const uint32_t* p);
    static int param_count(uint32_t cmd);
    void advance();
    void submit_vertex();
    void finish_polygon(const Vtx* v, int n);
    void rasterize(const Poly& p);
    void draw_triangle(const Poly& p, const Vtx& a, const Vtx& b, const Vtx& c);
    uint32_t texel(const Poly& p, int x, int y) const;
    void swap_buffers();

    const uint8_t* const* tex_ = nullptr; const uint8_t* const* pal_ = nullptr;
    int scale_ = 1;
    std::vector<uint32_t> color_; std::vector<float> depth_;
    uint32_t clear_color_ = 0, clear_depth_ = 0x7FFF;
    Mat proj_ = identity(), pos_ = identity(), tex_m_ = identity(), proj_stack_ = identity(), pos_stack_[32];
    int mode_ = 0, sp_ = 0;
    float vx_ = 0, vy_ = 0, vz_ = 0, cr_ = 1, cg_ = 1, cb_ = 1, ts_ = 0, tt_ = 0;
    uint32_t attr_ = 0, attr_latched_ = 0, tex_param_ = 0, pltt_base_ = 0;
    int vp_[4] = {0, 0, 255, 191};
    int prim_ = 0; Vtx assem_[4]; int assem_n_ = 0; bool strip_flip_ = false; int strip_count_ = 0;
    std::vector<Poly> polys_;
    // cola de comandos
    uint32_t packed_ = 0; int cmd_idx_ = 4; uint32_t cur_cmd_ = 0; int need_ = 0, have_ = 0; uint32_t params_[32];
    uint32_t port_params_[0x100][32]; int port_have_[0x100] = {};
};

}  // namespace nds
