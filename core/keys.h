// keys.h - Mascara de botones de DS (bit = 1 pulsado). Orden = registros KEYINPUT/EXTKEYIN.
#pragma once
#include <cstdint>
namespace nds {
enum Key : uint32_t {
    KEY_A = 1u << 0, KEY_B = 1u << 1, KEY_SELECT = 1u << 2, KEY_START = 1u << 3,
    KEY_RIGHT = 1u << 4, KEY_LEFT = 1u << 5, KEY_UP = 1u << 6, KEY_DOWN = 1u << 7,
    KEY_R = 1u << 8, KEY_L = 1u << 9, KEY_X = 1u << 10, KEY_Y = 1u << 11,
};
}  // namespace nds
