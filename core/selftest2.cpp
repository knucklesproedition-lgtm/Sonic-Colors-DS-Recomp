// Pruebas de: trucos de codigo sobre el interprete + Super Sonic.
// g++ -std=c++17 -O1 -fsanitize=address,undefined arm_cpu.cpp ds_system.cpp cheats.cpp mods.cpp selftest2.cpp -o selftest2
#include "ds_system.h"
#include "mods.h"
#include <cstdio>
#include <map>
using namespace nds;
static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; std::printf("FALLO linea %d: %s\n", __LINE__, #c); } } while (0)

struct FakeMem : MemIf {
    std::map<uint32_t, uint8_t> m;
    uint32_t rd(uint32_t a, int n) { uint32_t v = 0; for (int i = 0; i < n; i++) v |= (uint32_t)m[a + i] << (8 * i); return v; }
    void wr(uint32_t a, uint32_t v, int n) { for (int i = 0; i < n; i++) m[a + i] = (v >> (8 * i)) & 0xFF; }
    uint32_t r32(uint32_t a) override { return rd(a, 4); } uint32_t r16(uint32_t a) override { return rd(a, 2); } uint32_t r8(uint32_t a) override { return rd(a, 1); }
    void w32(uint32_t a, uint32_t v) override { wr(a, v, 4); } void w16(uint32_t a, uint32_t v) override { wr(a, v, 2); } void w8(uint32_t a, uint32_t v) override { wr(a, v, 1); }
};
// MemIf sobre la memoria REAL del ARM9 de la consola (asi los trucos tocan lo mismo que ve el interprete)
struct Arm9Mem : MemIf {
    Console& c; explicit Arm9Mem(Console& k) : c(k) {}
    uint32_t r32(uint32_t a) override { return c.bus9.read32(a & ~3u); } uint32_t r16(uint32_t a) override { return c.bus9.read16(a & ~1u); } uint32_t r8(uint32_t a) override { return c.bus9.read8(a); }
    void w32(uint32_t a, uint32_t v) override { c.bus9.write32(a & ~3u, v); } void w16(uint32_t a, uint32_t v) override { c.bus9.write16(a & ~1u, v); } void w8(uint32_t a, uint32_t v) override { c.bus9.write8(a, v); }
};

int main() {
    // ---- 1) Truco de CODIGO sobre el interprete ----
    // Programa en 0x02000000:  mov r0,#5 ; add r0,r0,#1 ; b .   (el truco cambia el "add" por "add r0,r0,#100")
    {
        std::unique_ptr<Console> cp(new Console); Console& c = *cp; Arm9Mem mem(c);
        c.bus9.write32(0x02000000, 0xE3A00005);       // mov r0,#5
        c.bus9.write32(0x02000004, 0xE2800001);       // add r0,r0,#1
        c.bus9.write32(0x02000008, 0xEAFFFFFE);       // b .
        c.cpu9.reset(0x02000000); c.cpu9.set_cpsr(0xDF, 1);
        for (int i = 0; i < 4; i++) c.cpu9.step();
        CHECK(c.cpu9.r[0] == 6);                       // sin truco
        CheatEngine e; std::vector<uint32_t> w; parse_code_text("02000004 E2800064", w);   // AR tipo 0: escribe la instruccion nueva
        e.add("sumar 100", w, true);
        e.apply_frame(mem);
        c.cpu9.reset(0x02000000); c.cpu9.set_cpsr(0xDF, 1);
        for (int i = 0; i < 4; i++) c.cpu9.step();
        CHECK(c.cpu9.r[0] == 105);                     // con truco: el interprete ejecuta el codigo parcheado
    }
    // ---- 2) Truco de DATOS sobre la RAM real ----
    { std::unique_ptr<Console> cp(new Console); Console& c = *cp; Arm9Mem mem(c); std::vector<uint32_t> w; parse_code_text("02001000 000003E7 12001010 0000BEEF", w);
      run_ar_code(w, mem); CHECK(c.bus9.read32(0x02001000) == 999 && c.bus9.read16(0x02001010) == 0xBEEF); }
    // ---- 3) Condicional por teclas (KEYINPUT real de la consola, activo en bajo) ----
    { std::unique_ptr<Console> cp(new Console); Console& c = *cp; Arm9Mem mem(c); std::vector<uint32_t> w; parse_code_text("94000130 FFFB0000 02002000 00000001", w);   // solo si Select
      c.keys = 0; run_ar_code(w, mem); CHECK(c.bus9.read32(0x02002000) == 0);
      c.keys = KEY_SELECT; run_ar_code(w, mem); CHECK(c.bus9.read32(0x02002000) == 1); }
    // ---- 4) Division por hardware del ARM9 (la usa el SDK) ----
    { std::unique_ptr<Console> cp(new Console); Console& c = *cp; c.bus9.write32(0x04000280, 0); c.bus9.write32(0x04000290, 1000); c.bus9.write32(0x04000294, 0); c.bus9.write32(0x04000298, 7); c.bus9.write32(0x0400029C, 0);
      CHECK(c.bus9.read32(0x040002A0) == 142 && c.bus9.read32(0x040002A8) == 6);
      c.bus9.write32(0x040002B8, 144); CHECK(c.bus9.read16(0x040002B4) == 12); }
    // ---- 5) FIFO entre ARM9 y ARM7 ----
    { std::unique_ptr<Console> cp(new Console); Console& c = *cp; c.bus9.write16(0x04000184, 0x8000); c.bus7.write16(0x04000184, 0x8000);
      c.bus9.write32(0x04000188, 0xCAFE0001); c.bus9.write32(0x04000188, 0xCAFE0002);
      CHECK((c.bus7.read16(0x04000184) & 0x100) == 0);                       // ARM7: cola de recepcion no vacia
      CHECK(c.bus7.read32(0x04100000) == 0xCAFE0001 && c.bus7.read32(0x04100000) == 0xCAFE0002);
      CHECK((c.bus7.read16(0x04000184) & 0x100) != 0); }
    // ---- 6) Super Sonic ----
    {
        FakeMem m; SuperSonicConfig cfg; cfg.rings_addr = 0x100; cfg.emeralds_addr = 0x110; cfg.ground_addr = 0x120; cfg.invincible_addr = 0x130;
        SuperSonicMod s(cfg); s.enabled = true;
        auto setup = [&](uint32_t rings, uint32_t em, uint32_t ground) { m.w16(0x100, rings); m.w8(0x110, em); m.w8(0x120, ground); m.w32(0x130, 0); };
        auto press = [&](SuperSonicMod& x) { x.on_frame(m, 0); x.on_frame(m, KEY_X); };
        setup(49, 0x7F, 0); press(s); CHECK(!s.active());                     // 49 anillos: no
        setup(50, 0x3F, 0); press(s); CHECK(!s.active());                     // faltan esmeraldas: no
        setup(50, 0x7F, 1); press(s); CHECK(!s.active());                     // en el suelo: no
        setup(50, 0x7F, 0); press(s); CHECK(s.active());                      // todo cumplido: si
        s.on_frame(m, 0); CHECK(m.r32(0x130) == 0x7FFFFFFF);                  // invencible
        for (int i = 0; i < 59; i++) s.on_frame(m, 0);
        CHECK(m.r16(0x100) == 49);                                            // un anillo por segundo
        for (int i = 0; i < 60 * 48; i++) s.on_frame(m, 0);
        CHECK(m.r16(0x100) == 1 && s.active());
        for (int i = 0; i < 60; i++) s.on_frame(m, 0);
        CHECK(m.r16(0x100) == 0 && !s.active() && m.r32(0x130) == 0);         // a 0: se destransforma
        SuperSonicConfig bad; SuperSonicMod nocfg(bad); nocfg.on_frame(m, KEY_X); CHECK(!nocfg.active());   // sin direcciones: inerte
        ModHost host; auto up = std::make_unique<SuperSonicMod>(cfg); up->enabled = false; host.add(std::move(up));
        setup(50, 0x7F, 0); host.run_frame(m, 0); host.run_frame(m, KEY_X);
        CHECK(!static_cast<SuperSonicMod*>(host.find("super_sonic"))->active());
    }
    std::printf("%d comprobaciones, %d fallos\n", checks, fails);
    return fails ? 1 : 0;
}
