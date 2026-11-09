#include "mods.h"
#include <cstring>

namespace nds {

Mod* ModHost::find(const char* n) { for (auto& m : mods_) if (!std::strcmp(m->name(), n)) return m.get(); return nullptr; }

void SuperSonicMod::on_frame(MemIf& mem, uint32_t keys) {
    if (!cfg.configured()) return;
    bool down = (keys & cfg.activate_key) != 0;
    bool pressed = down && !key_was_down_;                    // solo el flanco de subida
    key_was_down_ = down;

    if (!active_) {
        if (!pressed) return;
        uint32_t rings = rd(mem, cfg.rings_addr, cfg.ring_bytes);
        bool all_emeralds = (rd(mem, cfg.emeralds_addr, 1) & cfg.emeralds_mask) == cfg.emeralds_mask;
        bool in_air = (rd(mem, cfg.ground_addr, 1) & cfg.ground_mask) == 0;
        if (rings >= cfg.min_rings && all_emeralds && in_air) {
            active_ = true; counter_ = 0;
            saved_invincible_ = mem.r32(cfg.invincible_addr);
        }
        return;
    }
    mem.w32(cfg.invincible_addr, cfg.invincible_value);       // invencibilidad infinita mientras dure
    if (++counter_ >= cfg.frames_per_ring) {
        counter_ = 0;
        uint32_t rings = rd(mem, cfg.rings_addr, cfg.ring_bytes);
        if (rings > 0) wr(mem, cfg.rings_addr, rings - 1, cfg.ring_bytes);
        if (rings <= 1) {                                      // llego a 0: se destransforma
            active_ = false;
            mem.w32(cfg.invincible_addr, saved_invincible_ > cfg.invincible_value / 2 ? 0 : saved_invincible_);
        }
    }
}

}  // namespace nds
