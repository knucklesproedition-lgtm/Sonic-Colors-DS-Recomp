// Pruebas del motor 3D.  g++ -std=c++17 -O1 -fsanitize=address,undefined gpu3d.cpp selftest3.cpp -o selftest3
#include "gpu3d.h"
#include <cstdio>
#include <cstring>
using namespace nds;
static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; std::printf("FALLO linea %d: %s\n", __LINE__, #c); } } while (0)

static uint32_t fx(float v) { return (uint32_t)(int32_t)(v * 4096.0f); }
static uint32_t xy(float x, float y) { return (fx(x) & 0xFFFF) | (fx(y) << 16); }
static void P(Gpu3D& g, uint32_t cmd, std::initializer_list<uint32_t> ps = {}) { if (ps.size() == 0) g.port_write(cmd, 0); else for (uint32_t p : ps) g.port_write(cmd, p); }
static void vtx(Gpu3D& g, float x, float y, float z) { P(g, 0x23, {xy(x, y), fx(z) & 0xFFFF}); }
static uint32_t col5(int r, int gg, int b) { return r | (gg << 5) | (b << 10); }
static const uint32_t OPAQUE_BOTH = (31u << 16) | 0xC0;
static uint32_t px(Gpu3D& g, int x, int y) { return g.frame()[(size_t)y * g.width() + x]; }
static void tri(Gpu3D& g, float x0, float y0, float x1, float y1, float x2, float y2, float z, uint32_t color15, uint32_t attr = OPAQUE_BOTH) {
    P(g, 0x29, {attr}); P(g, 0x40, {0}); P(g, 0x20, {color15});
    vtx(g, x0, y0, z); vtx(g, x1, y1, z); vtx(g, x2, y2, z); P(g, 0x41);
}

int main() {
    // ---- 1) triangulo rojo, escala 1 ----
    { Gpu3D g; tri(g, -0.5f, -0.5f, 0.5f, -0.5f, 0.0f, 0.5f, 0.0f, col5(31, 0, 0)); P(g, 0x50, {0});
      CHECK(px(g, 128, 96) == 0xFFFF0000u); CHECK(px(g, 2, 2) == 0u); CHECK(px(g, 250, 190) == 0u); CHECK(g.polys_drawn == 1); }
    // ---- 2) escala: 2x da imagen 2x y cobertura ~4x ----
    { Gpu3D a, b; b.set_scale(2);
      tri(a, -0.5f, -0.5f, 0.5f, -0.5f, 0.0f, 0.5f, 0.0f, col5(0, 31, 0)); tri(b, -0.5f, -0.5f, 0.5f, -0.5f, 0.0f, 0.5f, 0.0f, col5(0, 31, 0));
      P(a, 0x50, {0}); P(b, 0x50, {0});
      auto cov = [](Gpu3D& g) { long n = 0; for (uint32_t v : g.frame()) n += (v >> 24) != 0; return n; };
      CHECK(b.width() == 512 && b.height() == 384 && b.frame().size() == 512u * 384u);
      double ratio = (double)cov(b) / (double)cov(a); CHECK(ratio > 3.8 && ratio < 4.2); }
    // ---- 3) culling: CW con "solo frontal" desaparece; con "ambas" se dibuja ----
    { Gpu3D g; tri(g, -0.5f, -0.5f, 0.0f, 0.5f, 0.5f, -0.5f, 0.0f, col5(31, 31, 31), (31u << 16) | 0x80); P(g, 0x50, {0}); CHECK(px(g, 128, 96) == 0u && g.polys_culled == 1);
      Gpu3D h; tri(h, -0.5f, -0.5f, 0.0f, 0.5f, 0.5f, -0.5f, 0.0f, col5(31, 31, 31), OPAQUE_BOTH); P(h, 0x50, {0}); CHECK(px(h, 128, 96) == 0xFFFFFFFFu);
      Gpu3D k; tri(k, -0.5f, -0.5f, 0.0f, 0.5f, 0.5f, -0.5f, 0.0f, col5(31, 31, 31), (31u << 16)); P(k, 0x50, {0}); CHECK(px(k, 128, 96) == 0u); }   // ni una cara: no se dibuja
    // ---- 4) profundidad: el mas cercano gana sin importar el orden ----
    { for (int order = 0; order < 2; order++) { Gpu3D g;
        if (order == 0) { tri(g, -0.8f, -0.8f, 0.8f, -0.8f, 0, 0.8f, 0.5f, col5(31, 0, 0)); tri(g, -0.8f, -0.8f, 0.8f, -0.8f, 0, 0.8f, -0.5f, col5(0, 0, 31)); }
        else          { tri(g, -0.8f, -0.8f, 0.8f, -0.8f, 0, 0.8f, -0.5f, col5(0, 0, 31)); tri(g, -0.8f, -0.8f, 0.8f, -0.8f, 0, 0.8f, 0.5f, col5(31, 0, 0)); }
        P(g, 0x50, {0}); CHECK(px(g, 128, 96) == 0xFF0000FFu); } }
    // ---- 5) transparencia: 50% de rojo sobre azul opaco ----
    { Gpu3D g; tri(g, -0.9f, -0.9f, 0.9f, -0.9f, 0, 0.9f, 0.5f, col5(0, 0, 31)); tri(g, -0.9f, -0.9f, 0.9f, -0.9f, 0, 0.9f, -0.5f, col5(31, 0, 0), (15u << 16) | 0xC0);
      P(g, 0x50, {0}); uint32_t v = px(g, 128, 96); int r = (v >> 16) & 255, b = v & 255; CHECK(r > 110 && r < 145 && b > 110 && b < 145 && (v >> 24) == 0xFF); }
    // ---- 6) matrices: traslacion + escala + push/pop ----
    { Gpu3D g; P(g, 0x10, {1}); P(g, 0x15); P(g, 0x1C, {fx(0.5f), 0, 0}); P(g, 0x11); P(g, 0x1B, {fx(0.2f), fx(0.2f), fx(1)});
      tri(g, -1, -1, 1, -1, 0, 1, 0, col5(31, 0, 31)); P(g, 0x12, {1});                                    // triangulo pequeno a la derecha
      P(g, 0x50, {0});
      CHECK(px(g, 128 + 64, 96) == 0xFFFF00FFu); CHECK(px(g, 128 - 64, 96) == 0u); CHECK(px(g, 128 + 64 + 40, 96) == 0u);
      // tras el POP solo queda la traslacion: un triangulo grande tapa el centro
      tri(g, -0.1f, -0.1f, 0.1f, -0.1f, 0, 0.1f, 0, col5(0, 31, 0)); P(g, 0x50, {0}); CHECK(px(g, 128 + 64, 96) == 0xFF00FF00u); }
    // ---- 7) recorte del plano cercano: vertice detras de la camara no rompe nada ----
    { Gpu3D g; P(g, 0x10, {0}); P(g, 0x15);
      // proyeccion "perspectiva" a mano: w = z (fila 3 = 0 0 1 0), z' = z
      P(g, 0x16, {fx(1),0,0,0, 0,fx(1),0,0, 0,0,fx(1),fx(1), 0,0,0,0});    // w = z ; (z' = z) ; Y z/w = 1 -> en el plano lejano
      P(g, 0x10, {1}); P(g, 0x15);
      tri(g, -0.5f, -0.5f, 0.5f, -0.5f, 0.0f, 0.5f, -0.5f, col5(31, 31, 0));    // z negativo: w<0 -> se descarta o se recorta sin colapsar
      P(g, 0x50, {0}); CHECK(g.frames == 1); }
    // ---- 8) texturas: 8x8 de 256 colores, mitad rojo / mitad verde ----
    { static uint8_t slot0[0x4000], palp[0x4000]; static const uint8_t* tp[32]; static const uint8_t* pp[6];
      std::memset(slot0, 0, sizeof slot0); std::memset(palp, 0, sizeof palp);
      for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) slot0[y * 8 + x] = x < 4 ? 1 : 2;
      palp[2] = 0x1F; palp[3] = 0x00;                         // indice 1 = rojo (0x001F)
      palp[4] = 0xE0; palp[5] = 0x03;                         // indice 2 = verde (0x03E0)
      for (int i = 0; i < 32; i++) tp[i] = nullptr; for (int i = 0; i < 6; i++) pp[i] = nullptr; tp[0] = slot0; pp[0] = palp;
      Gpu3D g; g.set_mem(tp, pp);
      P(g, 0x2A, {(4u << 26)});                                // formato 4, 8x8, sin repeticion, desplazamiento 0
      P(g, 0x2B, {0});
      P(g, 0x29, {OPAQUE_BOTH}); P(g, 0x40, {0}); P(g, 0x20, {col5(31, 31, 31)});
      auto tv = [&](float x, float y, float s, float t) { P(g, 0x22, {((uint32_t)(int)(s * 16) & 0xFFFF) | ((uint32_t)(int)(t * 16) << 16)}); vtx(g, x, y, 0); };
      tv(-1, -1, 0, 8); tv(1, -1, 8, 8); tv(1, 1, 8, 0);       // dos triangulos que cubren la pantalla
      tv(-1, -1, 0, 8); tv(1, 1, 8, 0); tv(-1, 1, 0, 0); P(g, 0x41); P(g, 0x50, {0});
      CHECK(px(g, 50, 96) == 0xFFFF0000u); CHECK(px(g, 200, 96) == 0xFF00FF00u); CHECK(g.textured_polys >= 1); }
    // ---- 9) cola empaquetada (FIFO): COLOR + 3 x VTX_16 en una palabra ----
    { Gpu3D g; g.port_write(0x29, OPAQUE_BOTH); g.port_write(0x40, 0);
      g.fifo_write(0x23232320u);                               // comandos (de menor a mayor byte): 0x20, 0x23, 0x23, 0x23
      g.fifo_write(col5(31, 31, 0));
      g.fifo_write(xy(-0.5f, -0.5f)); g.fifo_write(0); g.fifo_write(xy(0.5f, -0.5f)); g.fifo_write(0); g.fifo_write(xy(0.0f, 0.5f)); g.fifo_write(0);
      g.fifo_write(0x00000050u);                               // SWAP_BUFFERS empaquetado
      g.fifo_write(0);
      CHECK(px(g, 128, 96) == 0xFFFFFF00u && g.frames == 1); }
    // ---- 10) color de fondo ----
    { Gpu3D g; g.set_clear_color(col5(0, 0, 31) | (31u << 16)); P(g, 0x50, {0}); CHECK(px(g, 10, 10) == 0xFF0000FFu);
      Gpu3D h; h.set_clear_color(col5(0, 0, 31)); P(h, 0x50, {0}); CHECK(px(h, 10, 10) == 0u); }   // alfa 0: transparente
    // ---- 11) iluminacion difusa: luz de frente = blanco; luz por detras = solo ambiente (negro) ----
    for (int front = 0; front < 2; front++) {
        Gpu3D g; auto pk = [](float x, float y, float z) { return (uint32_t)(((int)(x * 511) & 0x3FF) | (((int)(y * 511) & 0x3FF) << 10) | (((int)(z * 511) & 0x3FF) << 20)); };
        P(g, 0x30, {0x7FFF});                                   // difuso blanco, ambiente 0
        P(g, 0x33, {0x7FFFu});                                  // luz 0 blanca
        P(g, 0x32, {pk(0, 0, front ? -1.0f : 1.0f)});           // direccion de la luz
        P(g, 0x29, {(31u << 16) | 0xC0 | 1});                   // luz 0 activa
        P(g, 0x40, {0}); P(g, 0x21, {pk(0, 0, 1)});             // normal hacia el observador
        vtx(g, -0.5f, -0.5f, 0); vtx(g, 0.5f, -0.5f, 0); vtx(g, 0.0f, 0.5f, 0); P(g, 0x41); P(g, 0x50, {0});
        uint32_t v = px(g, 128, 96); int rr = (v >> 16) & 255;
        if (front) CHECK(rr > 240 && (v >> 24) == 0xFF); else CHECK(rr < 8 && (v >> 24) == 0xFF);
    }
    // ---- 12) toon: el rojo del vertice indexa la tabla ----
    { Gpu3D g; g.set_toon(31, col5(31, 0, 0)); g.set_toon(0, col5(0, 0, 31));
      P(g, 0x29, {(31u << 16) | 0xC0 | (2u << 4)}); P(g, 0x40, {0}); P(g, 0x20, {col5(31, 31, 31)});
      vtx(g, -0.5f, -0.5f, 0); vtx(g, 0.5f, -0.5f, 0); vtx(g, 0.0f, 0.5f, 0); P(g, 0x41); P(g, 0x50, {0});
      CHECK(px(g, 128, 96) == 0xFFFF0000u);                      // indice 31 -> rojo
      Gpu3D h; h.set_toon(0, col5(0, 0, 31));
      P(h, 0x29, {(31u << 16) | 0xC0 | (2u << 4)}); P(h, 0x40, {0}); P(h, 0x20, {0});
      vtx(h, -0.5f, -0.5f, 0); vtx(h, 0.5f, -0.5f, 0); vtx(h, 0.0f, 0.5f, 0); P(h, 0x41); P(h, 0x50, {0});
      CHECK(px(h, 128, 96) == 0xFF0000FFu); }                    // indice 0 -> azul
    // ---- 13) salto de fotogramas: no rasteriza pero el estado sigue ----
    { Gpu3D g; g.skip = true; tri(g, -0.5f, -0.5f, 0.5f, -0.5f, 0.0f, 0.5f, 0.0f, col5(31, 0, 0)); P(g, 0x50, {0}); CHECK(px(g, 128, 96) == 0u && g.frames == 1);
      g.skip = false; tri(g, -0.5f, -0.5f, 0.5f, -0.5f, 0.0f, 0.5f, 0.0f, col5(31, 0, 0)); P(g, 0x50, {0}); CHECK(px(g, 128, 96) == 0xFFFF0000u); }
    std::printf("%d comprobaciones, %d fallos\n", checks, fails);
    return fails ? 1 : 0;
}
