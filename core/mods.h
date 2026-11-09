// mods.h - Capa de mods: codigo C++ que corre cada fotograma y puede leer/escribir la memoria del juego.
// Los trucos AR (cheats.h) y "Super Sonic" comparten esta infraestructura.
#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include "cheats.h"
#include "keys.h"

namespace nds {

class Mod {
public:
    virtual ~Mod() = default;
    virtual const char* name() const = 0;
    virtual void on_frame(MemIf& mem, uint32_t keys) = 0;    // keys = mascara nds::Key de este fotograma
    bool enabled = false;
};

class ModHost {
public:
    void add(std::unique_ptr<Mod> m) { mods_.push_back(std::move(m)); }
    void run_frame(MemIf& mem, uint32_t keys) { for (auto& m : mods_) if (m->enabled) m->on_frame(mem, keys); }
    Mod* find(const char* name);
private:
    std::vector<std::unique_ptr<Mod>> mods_;
};

// ---- Super Sonic ----
// Las direcciones hay que descubrirlas dentro del juego (ver README): mientras valgan 0 el mod no hace nada.
struct SuperSonicConfig {
    uint32_t rings_addr = 0;            // contador de anillos
    int ring_bytes = 2;                 // 1, 2 o 4
    uint32_t emeralds_addr = 0;         // byte/palabra con las banderas de esmeraldas
    uint32_t emeralds_mask = 0x7F;      // bits que deben estar todos a 1
    uint32_t ground_addr = 0;           // bandera "Sonic esta en el suelo"
    uint32_t ground_mask = 1;
    uint32_t invincible_addr = 0;       // temporizador de invencibilidad (u32)
    uint32_t invincible_value = 0x7FFFFFFF;
    uint32_t min_rings = 50;
    uint32_t frames_per_ring = 60;      // un anillo por segundo
    uint32_t activate_key = KEY_X;
    bool configured() const { return rings_addr && emeralds_addr && ground_addr && invincible_addr; }
};

class SuperSonicMod : public Mod {
public:
    explicit SuperSonicMod(const SuperSonicConfig& c) : cfg(c) {}
    const char* name() const override { return "super_sonic"; }
    void on_frame(MemIf& mem, uint32_t keys) override;
    bool active() const { return active_; }                  // el renderizador lo usa para teñir a Sonic de amarillo
    SuperSonicConfig cfg;
private:
    uint32_t rd(MemIf& m, uint32_t a, int bytes) { return bytes == 1 ? m.r8(a) : bytes == 2 ? m.r16(a) : m.r32(a); }
    void wr(MemIf& m, uint32_t a, uint32_t v, int bytes) { if (bytes == 1) m.w8(a, v); else if (bytes == 2) m.w16(a, v); else m.w32(a, v); }
    bool active_ = false, key_was_down_ = false;
    uint32_t counter_ = 0, saved_invincible_ = 0;
};

}  // namespace nds
