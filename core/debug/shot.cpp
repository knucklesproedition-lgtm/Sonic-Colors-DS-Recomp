// Ejecuta la ROM N unidades y guarda las dos pantallas como PPM (para verlas) + estadisticas.
#include "ds_system.h"
#include "cheats.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <tuple>
using namespace nds;
struct Arm9Mem : MemIf { Console& c; explicit Arm9Mem(Console& k) : c(k) {}
    uint32_t r32(uint32_t a) override { return c.bus9.read32(a & ~3u); } uint32_t r16(uint32_t a) override { return c.bus9.read16(a & ~1u); } uint32_t r8(uint32_t a) override { return c.bus9.read8(a); }
    void w32(uint32_t a, uint32_t v) override { c.bus9.write32(a & ~3u, v); } void w16(uint32_t a, uint32_t v) override { c.bus9.write16(a & ~1u, v); } void w8(uint32_t a, uint32_t v) override { c.bus9.write8(a, v); } };
static void save(const char* path, const uint32_t* px, int w, int h) {
    FILE* f = std::fopen(path, "wb"); std::fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int i = 0; i < w * h; i++) { uint32_t p = px[i]; uint8_t rgb[3] = {(uint8_t)p, (uint8_t)(p >> 8), (uint8_t)(p >> 16)}; std::fwrite(rgb, 1, 3, f); }
    std::fclose(f);
}
int main(int argc, char** argv) {
    std::ifstream f(argv[1], std::ios::binary); std::vector<uint8_t> rom((std::istreambuf_iterator<char>(f)), {});
    uint64_t units = atoll(argv[2]); const char* prefix = argc > 3 ? argv[3] : "/tmp/shot";
    static Console c; c.load_rom(rom); c.set_graphics(1, 0);
    std::vector<uint32_t> ar; bool have_ar = false; if (const char* e = std::getenv("SHOT_CHEAT")) have_ar = parse_code_text(e, ar);
    uint64_t chunk = have_ar ? 560190 : 2000000, done = 0;
    // pulsaciones: argv[5] = "tiempoM:duracionM:tecla" separadas por comas (tecla = mascara hex), p.ej. "600:5:8"
    std::vector<std::tuple<uint64_t,uint64_t,uint32_t>> presses; if (argc > 5) { std::string a = argv[5]; size_t p = 0; while (p < a.size()) { size_t e = a.find(',', p); if (e == std::string::npos) e = a.size(); unsigned t, d, k; if (std::sscanf(a.substr(p, e - p).c_str(), "%u:%u:%x", &t, &d, &k) == 3) presses.push_back({t * 1000000ull, d * 1000000ull, k}); p = e + 1; } }
    uint64_t period = argc > 4 ? atoll(argv[4]) * 1000000ull : 0, next_snap = period; int snap = 0;
    while (done < units) { c.keys = 0; for (auto& pr : presses) if (done >= std::get<0>(pr) && done < std::get<0>(pr) + std::get<1>(pr)) c.keys |= std::get<2>(pr);
        if (have_ar && c.arm9_booted()) { Arm9Mem mem(c); run_ar_code(ar, mem); }
        c.run_units(std::min(chunk, units - done)); done += chunk;
        if (period && done >= next_snap) { char q[256]; std::snprintf(q, sizeof q, "%s_%02d_top.ppm", prefix, snap); save(q, c.screen_pixels(0), c.screen_w(), c.screen_h()); std::snprintf(q, sizeof q, "%s_%02d_bottom.ppm", prefix, snap); save(q, c.screen_pixels(1), c.screen_w(), c.screen_h()); snap++; next_snap += period; }
        if (!have_ar || (done / chunk) % 40 == 0) std::printf("[%4llu M] ARM9 pc=%08x%s ARM7 pc=%08x%s  frames=%llu  cart B7=%llu  3D frames=%llu polys=%llu  unsup bg=%llu obj=%llu  DISPCNT A=%08x B=%08x B7=%llu\n", (unsigned long long)(done / 1000000), c.cpu9.pc(), c.cpu9.halted ? "(h)" : "", c.cpu7.pc(), c.cpu7.halted ? "(h)" : "",
            (unsigned long long)c.video_frames, (unsigned long long)c.card_cmd_count[0xB7], (unsigned long long)c.gpu3d.frames, (unsigned long long)c.gpu3d.polys_drawn, (unsigned long long)c.unsupported_bg, (unsigned long long)c.unsupported_obj, c.bus9.read32(0x04000000), c.bus9.read32(0x04001000), (unsigned long long)c.card_cmd_count[0xB7]); }
    char p[256]; std::snprintf(p, sizeof p, "%s_top.ppm", prefix); save(p, c.screen_pixels(0), c.screen_w(), c.screen_h());
    std::snprintf(p, sizeof p, "%s_bottom.ppm", prefix); save(p, c.screen_pixels(1), c.screen_w(), c.screen_h());
    std::printf("DISPCNT A=%08x B=%08x POWCNT1=%04x  VRAMCNT:", c.bus9.read32(0x04000000), c.bus9.read32(0x04001000), c.bus9.read16(0x04000304));
    for (int i = 0; i < 9; i++) std::printf(" %02x", c.bus9.read8(0x04000240 + (i < 7 ? i : i + 1))); std::printf("\n");
    std::printf("SWI sin soporte ARM9:"); for (auto& kv : c.swi_unhandled[0]) std::printf(" %02x x%llu", kv.first, (unsigned long long)kv.second);
    std::printf("  ARM7:"); for (auto& kv : c.swi_unhandled[1]) std::printf(" %02x x%llu", kv.first, (unsigned long long)kv.second); std::printf("\n");
    std::printf("cmds cartucho:"); for (auto& kv : c.card_cmd_count) std::printf(" %02x x%llu", kv.first, (unsigned long long)kv.second); std::printf("\n");
    std::vector<std::pair<uint64_t, uint32_t>> v; for (auto& kv : c.io_unknown[0]) v.push_back({kv.second, kv.first}); std::sort(v.rbegin(), v.rend());
    std::printf("E/S sin implementar ARM9 (top):"); for (size_t i = 0; i < v.size() && i < 10; i++) std::printf(" %05x x%llu", v[i].second, (unsigned long long)v[i].first); std::printf("\n");
    v.clear(); for (auto& kv : c.io_unknown[1]) v.push_back({kv.second, kv.first}); std::sort(v.rbegin(), v.rend());
    std::printf("E/S sin implementar ARM7 (top):"); for (size_t i = 0; i < v.size() && i < 10; i++) std::printf(" %05x x%llu", v[i].second, (unsigned long long)v[i].first); std::printf("\n");
}
