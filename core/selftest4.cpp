// Pruebas de video: bancos de VRAM, fondos de tiles, sprites, prioridades y 3D como fondo 0.
// g++ -std=c++17 -O1 -fsanitize=address,undefined arm_cpu.cpp ds_system.cpp ds_video.cpp gpu3d.cpp selftest4.cpp -o selftest4
#include "ds_system.h"
#include <cstdio>
#include <memory>
using namespace nds;
static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; std::printf("FALLO linea %d: %s\n", __LINE__, #c); } } while (0)

static std::unique_ptr<Console> mk() { return std::unique_ptr<Console>(new Console()); }
static uint32_t top(Console& c, int x, int y) { return c.screen_pixels(0)[(size_t)y * c.screen_w() + x]; }
static const uint32_t RED = 0xFF0000FFu, GREEN = 0xFF00FF00u, BLUE = 0xFFFF0000u, BLACK = 0xFF000000u;   // ABGR (byte 0 = R)
static uint32_t fx(float v) { return (uint32_t)(int32_t)(v * 4096.0f); }
static uint32_t xy(float x, float y) { return (fx(x) & 0xFFFF) | (fx(y) << 16); }

static void hide_sprites(Console& c) { for (int i = 1; i < 128; i++) c.bus9.write16(0x07000000 + i * 8, 0x200); }   // una OAM a cero = 127 sprites visibles

static void bg_setup(Console& c, int bg0_prio) {
    c.bus9.write8(0x04000240, 0x81);                                  // banco A -> fondos del motor A
    c.bus9.write16(0x04000304, 0x8000);                               // motor A en la pantalla superior
    c.bus9.write16(0x04000008, 0x84 | bg0_prio);                      // BG0: base de tiles 1, 256 colores, mapa en 0
    c.bus9.write16(0x06000000, 1);                                    // casilla (0,0) = tile 1
    for (int i = 0; i < 64; i++) c.bus9.write8(0x06004040 + i, 5);    // tile 1: todo indice 5
    c.bus9.write16(0x05000000 + 10, 0x001F);                          // indice 5 = rojo
    c.bus9.write32(0x04000000, 0x00010100);                           // modo 0, BG0 activo, mostrar graficos
}

int main() {
    // ---- 1) mapeo de VRAM: LCDC -> fondo, y el dato sigue ahi ----
    { auto c = mk(); c->bus9.write8(0x04000240, 0x80); c->bus9.write32(0x06800010, 0xCAFEBABE); CHECK(c->bus9.read32(0x06800010) == 0xCAFEBABE);
      c->bus9.write8(0x04000240, 0x81); CHECK(c->bus9.read32(0x06000010) == 0xCAFEBABE && c->bus9.read32(0x06800010) == 0);
      c->bus9.write8(0x04000240, 0x89); CHECK(c->bus9.read32(0x06020010) == 0xCAFEBABE);                  // offset 1 -> +128 KB
      c->bus9.write8(0x04000240, 0x00); CHECK(c->bus9.read32(0x06000010) == 0); }                          // desactivado: no mapeado
    // ---- 2) fondo de tiles ----
    { auto c = mk(); bg_setup(*c, 0); c->render_now();
      CHECK(top(*c, 0, 0) == RED && top(*c, 7, 7) == RED && top(*c, 8, 0) == BLACK && top(*c, 0, 8) == BLACK);
      c->bus9.write16(0x04000010, 4);                                 // desplazamiento horizontal de 4: el tile aparece 4 px a la izquierda
      c->render_now(); CHECK(top(*c, 3, 0) == RED && top(*c, 4, 0) == BLACK); }
    // ---- 3) sprite 8x8 de 4 bits ----
    { auto c = mk(); c->bus9.write8(0x04000241, 0x82); c->bus9.write16(0x04000304, 0x8000);
      for (int i = 0; i < 32; i++) c->bus9.write8(0x06400000 + i, 0x11);
      c->bus9.write16(0x05000200 + 2, 0x03E0);                        // paleta de objetos, indice 1 = verde
      hide_sprites(*c); c->bus9.write16(0x07000000, 10); c->bus9.write16(0x07000002, 20); c->bus9.write16(0x07000004, 0);
      c->bus9.write32(0x04000000, 0x00011010); c->render_now();
      CHECK(top(*c, 20, 10) == GREEN && top(*c, 27, 17) == GREEN && top(*c, 19, 10) == BLACK && top(*c, 28, 10) == BLACK && top(*c, 20, 9) == BLACK && top(*c, 20, 18) == BLACK); }
    // ---- 4) prioridades fondo / sprite ----
    { for (int obj_prio = 0; obj_prio < 2; obj_prio++) { auto c = mk(); bg_setup(*c, 0);
        c->bus9.write8(0x04000241, 0x82); for (int i = 0; i < 32; i++) c->bus9.write8(0x06400000 + i, 0x11);
        c->bus9.write16(0x05000200 + 2, 0x03E0);
        hide_sprites(*c); c->bus9.write16(0x07000000, 0); c->bus9.write16(0x07000002, 0); c->bus9.write16(0x07000004, (uint16_t)(obj_prio << 10));
        c->bus9.write32(0x04000000, 0x00011110);                      // BG0 + sprites
        c->render_now();
        CHECK(top(*c, 2, 2) == (obj_prio == 0 ? GREEN : RED)); } }    // misma prioridad: gana el sprite; sprite con prioridad 1: gana el fondo
    // ---- 5) el motor B va en la pantalla inferior; intercambio con POWCNT1 ----
    { auto c = mk(); bg_setup(*c, 0); c->render_now(); CHECK(c->screen_pixels(1)[0] == 0xFFFFFFFFu || c->screen_pixels(1)[0] == BLACK);
      c->bus9.write16(0x04000304, 0x0000); c->render_now(); CHECK(c->screen_pixels(1)[0] == RED && top(*c, 0, 0) != RED); }
    // ---- 6) brillo maestro: modo 1 = subir, modo 2 = bajar ----
    { auto c = mk(); bg_setup(*c, 0); c->bus9.write16(0x0400006C, (1 << 14) | 16); c->render_now(); CHECK(top(*c, 0, 0) == 0xFFFFFFFFu);
      c->bus9.write16(0x0400006C, (2 << 14) | 16); c->render_now(); CHECK(top(*c, 0, 0) == BLACK);
      c->bus9.write16(0x0400006C, (2 << 14) | 8); c->render_now(); CHECK(top(*c, 0, 0) == 0xFF00007Fu || top(*c, 0, 0) == 0xFF000080u || (top(*c, 0, 0) & 0xFF) == 0x7F || (top(*c, 0, 0) & 0xFF) == 0x80); }
    // ---- 7) 3D como fondo 0, escala 1 y 2 ----
    for (int scale = 1; scale <= 2; scale++) {
        auto c = mk(); c->set_graphics(scale, 0); c->bus9.write16(0x04000304, 0x8000);
        auto P = [&](uint32_t cmd, uint32_t v) { c->bus9.write32(0x04000400 + cmd * 4, v); };
        P(0x29, (31u << 16) | 0xC0); P(0x40, 0); P(0x20, 31);                                  // color rojo
        P(0x23, xy(-0.5f, -0.5f)); P(0x23, 0); P(0x23, xy(0.5f, -0.5f)); P(0x23, 0); P(0x23, xy(0, 0.5f)); P(0x23, 0); P(0x50, 0);
        c->bus9.write16(0x04000008, 0x0000); c->bus9.write32(0x04000000, 0x00010108);
        c->render_now();
        CHECK(c->screen_w() == 256 * scale);
        CHECK(top(*c, 128 * scale, 96 * scale) == RED); CHECK(top(*c, 4 * scale, 4 * scale) == BLACK);
    }
    // ---- 8) 3D (BG0) frente a un fondo de tiles (BG1) segun prioridad ----
    for (int case_ = 0; case_ < 2; case_++) {
        auto c = mk(); c->bus9.write16(0x04000304, 0x8000); c->bus9.write8(0x04000240, 0x81);
        c->bus9.write16(0x0400000A, 0x184 | (case_ == 0 ? 1 : 0));                     // BG1: tiles, mapa en 0x800
        c->bus9.write16(0x06000800, 1); for (int i = 0; i < 64; i++) c->bus9.write8(0x06004040 + i, 5); c->bus9.write16(0x05000000 + 10, 0x001F);
        c->bus9.write16(0x04000008, case_ == 0 ? 0 : 1);                                // BG0 = 3D
        auto P = [&](uint32_t cmd, uint32_t v) { c->bus9.write32(0x04000400 + cmd * 4, v); };
        P(0x29, (31u << 16) | 0xC0); P(0x40, 0); P(0x20, 31 << 10);                    // 3D azul, triangulo enorme que cubre la esquina superior izquierda
        P(0x23, xy(-1.9f, 1.9f)); P(0x23, 0); P(0x23, xy(-1.9f, -1.9f)); P(0x23, 0); P(0x23, xy(1.9f, 1.9f)); P(0x23, 0); P(0x50, 0);
        c->bus9.write32(0x04000000, 0x00010308); c->render_now();
        CHECK(top(*c, 2, 2) == (case_ == 0 ? BLUE : RED));                              // 3D prioridad 0 / tiles 1 -> azul; 3D 1 / tiles 0 -> rojo
    }
    // ---- 9) texturas a traves de bancos reales: C->textura, E->paleta de texturas ----
    { auto c = mk(); c->bus9.write16(0x04000304, 0x8000);
      c->bus9.write8(0x04000242, 0x80); c->bus9.write8(0x04000244, 0x80);                        // banco C y E como LCDC para poder escribirlos
      for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) c->bus9.write8(0x06840000 + y * 8 + x, x < 4 ? 1 : 2);
      c->bus9.write16(0x06880000 + 2, 0x001F); c->bus9.write16(0x06880000 + 4, 0x03E0);          // paleta: 1 rojo, 2 verde
      c->bus9.write8(0x04000242, 0x83); c->bus9.write8(0x04000244, 0x83);                        // C = textura (ranura 0), E = paleta de texturas
      auto P = [&](uint32_t cmd, uint32_t v) { c->bus9.write32(0x04000400 + cmd * 4, v); };
      P(0x2A, 4u << 26); P(0x2B, 0); P(0x29, (31u << 16) | 0xC0); P(0x40, 0); P(0x20, 0x7FFF);
      auto tv = [&](float x, float y, float s, float t) { P(0x22, ((uint32_t)(int)(s * 16) & 0xFFFF) | ((uint32_t)(int)(t * 16) << 16)); P(0x23, xy(x, y)); P(0x23, 0); };
      tv(-1, -1, 0, 8); tv(1, -1, 8, 8); tv(1, 1, 8, 0); tv(-1, -1, 0, 8); tv(1, 1, 8, 0); tv(-1, 1, 0, 0); P(0x50, 0);
      c->bus9.write16(0x04000008, 0); c->bus9.write32(0x04000000, 0x00010108); c->render_now();
      CHECK(top(*c, 50, 96) == RED && top(*c, 200, 96) == GREEN); }
    std::printf("%d comprobaciones, %d fallos\n", checks, fails);
    return fails ? 1 : 0;
}
