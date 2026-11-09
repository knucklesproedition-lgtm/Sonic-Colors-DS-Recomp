// Pruebas en PC:  g++ -std=c++17 -O1 -fsanitize=address,undefined cheats.cpp layout.cpp selftest.cpp -o selftest && ./selftest
#include "cheats.h"
#include "layout.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
using namespace nds;

static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; std::printf("FALLO linea %d: %s\n", __LINE__, #c); } } while (0)

struct FakeMem : MemIf {
    std::map<uint32_t, uint8_t> m;
    uint32_t rd(uint32_t a, int n) { uint32_t v = 0; for (int i = 0; i < n; i++) v |= (uint32_t)m[a + i] << (8 * i); return v; }
    void wr(uint32_t a, uint32_t v, int n) { for (int i = 0; i < n; i++) m[a + i] = (v >> (8 * i)) & 0xFF; }
    uint32_t r32(uint32_t a) override { return rd(a, 4); }
    uint32_t r16(uint32_t a) override { return rd(a, 2); }
    uint32_t r8(uint32_t a) override { return rd(a, 1); }
    void w32(uint32_t a, uint32_t v) override { wr(a, v, 4); }
    void w16(uint32_t a, uint32_t v) override { wr(a, v, 2); }
    void w8(uint32_t a, uint32_t v) override { wr(a, v, 1); }
};
static std::vector<uint32_t> code(const char* t) { std::vector<uint32_t> w; bool ok = parse_code_text(t, w); CHECK(ok); return w; }

int main() {
    // ---- parser de codigo ----
    std::vector<uint32_t> w;
    CHECK(parse_code_text("94000130 FFFB0000\n12345678 9ABCDEF0", w) && w.size() == 4 && w[0] == 0x94000130u);
    CHECK(parse_code_text("9400013 0FFFB0000", w) && w.size() == 2);      // sin espacios exactos, mismos 16 digitos
    CHECK(!parse_code_text("94000130", w));                               // impar
    CHECK(!parse_code_text("hola mundo", w));

    // ---- escrituras 0/1/2 ----
    { FakeMem m; run_ar_code(code("02000000 11223344 12000010 0000BEEF 22000020 000000AB"), m);
      CHECK(m.r32(0x02000000) == 0x11223344u); CHECK(m.r16(0x02000010) == 0xBEEF); CHECK(m.r8(0x02000020) == 0xAB); }
    // ---- condicional 5 (igual): si es falso se salta TODO hasta D0/D2, no solo la linea siguiente ----
    { FakeMem m; m.w32(0x02000100, 5);
      run_ar_code(code("52000100 00000005 02000200 00000001 02000204 00000002"), m);       // verdadero: ejecuta todo
      CHECK(m.r32(0x02000200) == 1 && m.r32(0x02000204) == 2);
      FakeMem m2; m2.w32(0x02000100, 6);
      run_ar_code(code("52000100 00000005 02000200 00000001 02000204 00000002"), m2);       // falso sin D0: se salta TODO
      CHECK(m2.r32(0x02000200) == 0 && m2.r32(0x02000204) == 0);
      FakeMem m3; m3.w32(0x02000100, 6);
      run_ar_code(code("52000100 00000005 02000200 00000001 D0000000 00000000 02000204 00000002"), m3);   // D0 cierra el bloque
      CHECK(m3.r32(0x02000200) == 0 && m3.r32(0x02000204) == 2);
      // E dentro de un bloque falso: tambien se saltan sus lineas de datos
      FakeMem m4; m4.w32(0x02000100, 6);
      run_ar_code(code("52000100 00000005 E2004000 00000008 04030201 08070605 D2000000 00000000 02000500 00000007"), m4);
      CHECK(m4.r8(0x02004000) == 0 && m4.r32(0x02000500) == 7); }
    // ---- el codigo de pantalla unica del usuario: solo parchea si la memoria tiene la instruccion ORIGINAL ----
    { const char* ss = "5202B738 E5923028 0202A558 03A0047F 0202A574 03A0047F 020144B8 E1A00000 0202B634 E1A01003 D2000000 00000000";
      FakeMem comprimido; comprimido.w32(0x0202B738, 0x12345678);                              // aun sin descomprimir: no debe tocar nada
      run_ar_code(code(ss), comprimido);
      CHECK(comprimido.r32(0x0202A558) == 0 && comprimido.r32(0x020144B8) == 0 && comprimido.r32(0x0202B634) == 0);
      FakeMem listo; listo.w32(0x0202B738, 0xE5923028);                                        // codigo original presente: parchea las 4 direcciones
      run_ar_code(code(ss), listo);
      CHECK(listo.r32(0x0202A558) == 0x03A0047F && listo.r32(0x0202A574) == 0x03A0047F && listo.r32(0x020144B8) == 0xE1A00000 && listo.r32(0x0202B634) == 0xE1A01003); }
    // ---- condicional de teclas tipo 9 con mascara (KEYINPUT activo en bajo; Select = bit 2) ----
    { FakeMem m; m.w16(0x04000130, 0x03FF);                        // nada pulsado
      run_ar_code(code("94000130 FFFB0000 02000300 00000009"), m); CHECK(m.r32(0x02000300) == 0);
      m.w16(0x04000130, 0x03FB);                                    // Select pulsado
      run_ar_code(code("94000130 FFFB0000 02000300 00000009"), m); CHECK(m.r32(0x02000300) == 9); }
    // ---- offset (D3) y registro de datos (D5/D6) ----
    { FakeMem m; run_ar_code(code("D3000000 02001000 00000010 AAAAAAAA D2000000 00000000 00000020 BBBBBBBB"), m);
      CHECK(m.r32(0x02001010) == 0xAAAAAAAAu); CHECK(m.r32(0x00000020) == 0xBBBBBBBBu);
      FakeMem m2; run_ar_code(code("D5000000 11111111 D6000000 02002000 D6000000 02002000"), m2);
      CHECK(m2.r32(0x02002000) == 0x11111111u && m2.r32(0x02002004) == 0x11111111u); }
    // ---- bucle C ... D1 ----
    { FakeMem m; run_ar_code(code("C0000000 00000003 D4000000 00000002 D1000000 00000000 D6000000 02003000"), m);
      CHECK(m.r32(0x02003000) == 6); }
    // ---- E: parche de bytes ----
    { FakeMem m; run_ar_code(code("E2004000 00000005 04030201 00000005"), m);
      CHECK(m.r8(0x02004000) == 1 && m.r8(0x02004003) == 4 && m.r8(0x02004004) == 5); }
    // ---- lector de archivos ----
    { auto v = parse_cheat_file("cheat0_desc = \"Vidas infinitas\"\ncheat0_code = \"02000000 00000063\"\ncheat0_enable = false\n"
                                "cheat1_name = \"Una pantalla\"\ncheat1_code = \"94000130 FFFB0000 02000300 00000009\"\ncheat1_enable = true\n");
      CHECK(v.size() == 2 && v[0].name == "Vidas infinitas" && !v[0].enabled && v[1].enabled && v[1].words.size() == 4); }
    { auto v = parse_cheat_file("# comentario\n[Anillos infinitos]\n02000000 00000063\n\nVida\n12000010 0000FFFF\n02000004 00000001\n");
      CHECK(v.size() == 2 && v[0].name == "Anillos infinitos" && v[1].name == "Vida" && v[1].words.size() == 4); }
    CHECK(parse_cheat_file("esto no es un archivo de trucos\n").empty());

    // ---- motor ----
    { CheatEngine e; FakeMem m; int a = e.add("x", code("02000000 00000001"), false);
      e.add("y", code("02000004 00000002"), true);
      e.apply_frame(m); CHECK(m.r32(0x02000000) == 0 && m.r32(0x02000004) == 2);
      e.set_enabled(a, true); e.apply_frame(m); CHECK(m.r32(0x02000000) == 1); }

    // ---- layout ----
    const float W = 1080, H = 1440;
    { Layout l = compute_layout(W, H, LAYOUT_STACKED, false, 0.65f);
      CHECK(l.top.visible && l.bottom.visible && l.top.y < l.bottom.y);
      CHECK(l.top.w == l.bottom.w && l.top.y + l.top.h <= l.bottom.y + 0.01f && l.bottom.y + l.bottom.h <= H + 0.01f);
      Layout s = compute_layout(W, H, LAYOUT_STACKED, true, 0.65f);
      CHECK(s.bottom.y < s.top.y);                                    // swap: la tactil arriba
      int x, y; CHECK(map_touch(s, s.bottom.x + s.bottom.w / 2, s.bottom.y + s.bottom.h / 2, &x, &y) && x >= 126 && x <= 129 && y >= 94 && y <= 97);
      CHECK(!map_touch(s, s.top.x + 5, s.top.y + 5, &x, &y)); }       // tocar la otra pantalla no cuenta
    { Layout b = compute_layout(W, H, LAYOUT_BIG_TOP, false, 0.7f);
      CHECK(b.top.w > b.bottom.w && b.top.y < b.bottom.y);
      Layout b2 = compute_layout(W, H, LAYOUT_BIG_BOTTOM, false, 0.7f);
      CHECK(b2.bottom.w > b2.top.w);
      Layout b3 = compute_layout(W, H, LAYOUT_BIG_BOTTOM, true, 0.7f);
      CHECK(b3.bottom.w > b3.top.w && b3.bottom.y < b3.top.y);        // grande y ademas intercambiada
      CHECK(b.top.x >= 0 && b.top.x + b.top.w <= W + 0.01f); }
    { Layout o = compute_layout(W, H, LAYOUT_SINGLE_TOP, false, 0.65f); CHECK(o.top.visible && !o.bottom.visible && o.top.w <= W + 0.01f);
      Layout t = compute_layout(W, H, LAYOUT_SINGLE_BOTTOM, false, 0.65f); int x, y;
      CHECK(!t.top.visible && t.bottom.visible && map_touch(t, W / 2, H / 2, &x, &y)); }
    { Layout z = compute_layout(0, 0, LAYOUT_STACKED, false, 0.65f); int x, y; CHECK(!map_touch(z, 0, 0, &x, &y)); }   // vista vacia

    // ---- layout HORIZONTAL (pantallas una al lado de la otra) ----
    { const float LW = 2400, LH = 1080;
      Layout a = compute_layout(LW, LH, LAYOUT_STACKED, false, 0.65f);
      CHECK(a.top.visible && a.bottom.visible && a.top.x < a.bottom.x && a.top.w == a.bottom.w);
      CHECK(a.top.x >= 0 && a.bottom.x + a.bottom.w <= LW + 0.01f && a.top.h <= LH + 0.01f && std::abs((a.top.y + a.top.h / 2) - LH / 2) < 1.f);
      Layout s = compute_layout(LW, LH, LAYOUT_STACKED, true, 0.65f); CHECK(s.bottom.x < s.top.x);                  // intercambiar: la tactil a la izquierda
      Layout g = compute_layout(LW, LH, LAYOUT_BIG_BOTTOM, false, 0.7f); CHECK(g.bottom.w > g.top.w && g.top.x < g.bottom.x && g.bottom.x + g.bottom.w <= LW + 0.01f);
      Layout g2 = compute_layout(LW, LH, LAYOUT_BIG_TOP, true, 0.7f); CHECK(g2.top.w > g2.bottom.w && g2.bottom.x < g2.top.x);   // grande y ademas intercambiada
      int x, y; CHECK(map_touch(a, a.bottom.x + a.bottom.w / 2, a.bottom.y + a.bottom.h / 2, &x, &y) && x >= 126 && x <= 129 && y >= 94 && y <= 97);
      CHECK(!map_touch(a, a.top.x + 5, a.top.y + 5, &x, &y));
      Layout one = compute_layout(LW, LH, LAYOUT_SINGLE_BOTTOM, false, 0.65f); CHECK(one.bottom.visible && !one.top.visible && one.bottom.h <= LH + 0.01f); }
    std::printf("%d comprobaciones, %d fallos\n", checks, fails);
    return fails ? 1 : 0;
}
