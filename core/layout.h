// layout.h - Disposicion de las dos pantallas de DS (tipo DraStic) y mapeo del toque.
#pragma once

namespace nds {

struct Rect { float x = 0, y = 0, w = 0, h = 0; bool visible = false; };

enum LayoutMode {
    LAYOUT_STACKED = 0,        // dos pantallas iguales, una sobre otra
    LAYOUT_BIG_TOP = 1,        // la pantalla superior de DS grande, la inferior pequena
    LAYOUT_BIG_BOTTOM = 2,     // la inferior de DS (tactil) grande, la superior pequena
    LAYOUT_SINGLE_TOP = 3,     // solo la pantalla superior de DS
    LAYOUT_SINGLE_BOTTOM = 4,  // solo la pantalla inferior de DS
    LAYOUT_COUNT = 5
};

struct Layout { Rect top, bottom; };  // top = pantalla superior de DS, bottom = la tactil

// view_w/h: tamano del area de pantallas en pixeles.
// swapped: intercambia la posicion (la tactil arriba, la otra abajo), como "Swap screens" de DraStic.
// big_ratio: fraccion de la altura (0.5..0.85) para la pantalla grande en los modos BIG_*.
Layout compute_layout(float view_w, float view_h, int mode, bool swapped, float big_ratio);

// Convierte un toque de la vista a coordenadas de la pantalla tactil (0..255, 0..191).
// Devuelve false si el toque cae fuera de la pantalla tactil.
bool map_touch(const Layout& l, float x, float y, int* ds_x, int* ds_y);

}  // namespace nds
