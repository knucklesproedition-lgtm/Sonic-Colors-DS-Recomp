// Pruebas de video: bancos de VRAM, fondos de tiles, sprites, prioridades y 3D como fondo 0.
// g++ -std=c++17 -O1 -fsanitize=address,undefined arm_cpu.cpp ds_system.cpp ds_video.cpp gpu3d.cpp selftest4.cpp -o selftest4
#include "ds_system.h"
#include <cstdio>
#include <cstdlib>
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
    // ---- 10) sonido: PCM8 en bucle, volumen y panorama ----
    { auto c = mk(); uint32_t B = 0x02001000;
      for (int i = 0; i < 64; i++) c->bus7.write8(B + i, i < 32 ? 0x40 : 0xC0);                // cuadrada: +0x40 / -0x40
      c->bus7.write16(0x04000500, 0x8000 | 127);                                                // sonido activado, volumen maestro maximo
      c->bus7.write32(0x04000404, B); c->bus7.write16(0x04000408, 0x10000 - 512); c->bus7.write16(0x0400040A, 0); c->bus7.write32(0x0400040C, 16);   // fuente, timer (paso = 1), bucle 0, 16 palabras = 64 muestras
      c->bus7.write32(0x04000400, 127u | (127u << 16) | (1u << 27) | (0u << 29) | (1u << 31));   // volumen 127, panorama a la derecha, bucle, PCM8, iniciar
      CHECK((c->bus7.read16(0x04000402) & 0x8000) != 0);                                        // ocupado
      c->run_units(1024 * 70);
      int16_t buf[200 * 2]; size_t n = c->audio_read(buf, 200);
      CHECK(n >= 64);
      int pos = 0, neg = 0, rl = 0, rr = 0; for (size_t i = 0; i < n; i++) { if (buf[i * 2 + 1] > 1000) pos++; if (buf[i * 2 + 1] < -1000) neg++; rl += std::abs(buf[i * 2]); rr += std::abs(buf[i * 2 + 1]); }
      CHECK(pos > 20 && neg > 20);                                                              // alterna entre positivo y negativo
      CHECK(rr > rl * 20);                                                                      // panorama 127: casi todo por la derecha
      c->bus7.write32(0x04000400, 0);                                                           // detener
      CHECK((c->bus7.read16(0x04000402) & 0x8000) == 0); }
    // ---- 11) sonido: ADPCM (el valor inicial se refleja) y PSG (cuadrada con ciclo de trabajo) ----
    { auto c = mk(); uint32_t B = 0x02002000;
      c->bus7.write32(B, 0x00004000u);                                                           // cabecera: valor inicial 0x4000, indice 0
      for (int i = 0; i < 28; i++) c->bus7.write8(B + 4 + i, 0x00);                              // todos los nibbles a 0: casi sin cambio
      c->bus7.write16(0x04000500, 0x8000 | 127);
      c->bus7.write32(0x04000414, B); c->bus7.write16(0x04000418, 0x10000 - 512); c->bus7.write16(0x0400041A, 1); c->bus7.write32(0x0400041C, 7);
      c->bus7.write32(0x04000410, 127u | (64u << 16) | (2u << 27) | (2u << 29) | (1u << 31));
      c->run_units(1024 * 12); int16_t buf[40 * 2]; size_t n = c->audio_read(buf, 40); CHECK(n >= 12 && buf[2 * 8] > 4000 && buf[2 * 8] < 9000);   // ~0x4000 * 127/128 * mitad por oido
      auto d = mk();                                                                              // PSG canal 8, ciclo de trabajo 3 (50 %)
      d->bus7.write16(0x04000500, 0x8000 | 127); d->bus7.write16(0x04000488, 0x10000 - 512);
      d->bus7.write32(0x04000480, 127u | (64u << 16) | (3u << 24) | (3u << 29) | (1u << 31));
      d->run_units(1024 * 80); int16_t b2[80 * 2]; size_t m = d->audio_read(b2, 80); int hi = 0, lo = 0; for (size_t i = 0; i < m; i++) { if (b2[i * 2] > 1000) hi++; if (b2[i * 2] < -1000) lo++; }
      CHECK(m >= 64 && hi > 20 && lo > 20 && std::abs(hi - lo) <= 12); }
    std::printf("%d comprobaciones, %d fallos\n", checks, fails);
    return fails ? 1 : 0;
}
