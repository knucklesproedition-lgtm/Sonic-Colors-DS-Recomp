# Sonic Colors DS (USA) -> Android: proyecto de recomp

Necesitas **tu propia ROM** (`Sonic Colors (USA) (En,Ja,Fr,De,Es,It).nds`, codigo BXSE,
SHA-1 `fc76c4f3480b00f02646acb88d6713baf1b1d62a`). No incluyas la ROM, el codigo ni los
assets originales en ningun repositorio ni APK que distribuyas.

## Estado
| Pieza | Estado |
|---|---|
| Extractor (ARM9/ARM7/overlays/NitroFS) Python y C++ | Hecho y verificado (identicos byte a byte) |
| Comprobacion de version (gamecode + SHA-1) | Hecho |
| App Android: menu, selector de ROM, extraccion en el movil | Escrito, **sin compilar** (aqui no hay Android SDK) |
| Botones en pantalla (cruceta, A/B/X/Y, L/R, Start/Select, multitoque) | Escrito, sin compilar |
| Pantallas tipo DraStic: iguales, una grande/otra pequena, solo una, intercambiar | Logica en C++ probada; interfaz Kotlin sin compilar |
| Ajustes (disposicion, tamano de pantalla grande, botones, opacidad) | Escrito, sin compilar |
| Menu de trucos Action Replay (importar, anadir, activar) + motor AR | Motor y lector probados en PC; interfaz sin compilar |
| Lifter ARM->C | Prototipo: 91.3% de instrucciones del ARM9 |
| Thumb, ldm/stm generales, tablas de switch, coprocesador | Pendiente |
| Capa de hardware DS (memoria, DMA, timers, IPC ARM7) | Pendiente |
| GPU 2D/3D en OpenGL ES, audio SDAT, video MODS | Pendiente |

## Carpetas
- `tools/` Python (PC): `nds_extract.py`, `arm_lift.py`, `lift_selftest.py`
- `core/` C++ compartido: `nds_rom.cpp/.h` (lo usa la app Android)
- `android/` proyecto de Android Studio

## Probar el extractor en PC
    python3 tools/nds_extract.py rom.nds salida/
    g++ -std=c++17 -O2 core/nds_rom.cpp core/extract_main.cpp -o nds_extract && ./nds_extract rom.nds salida/

## Probar layout de pantallas y trucos (PC)
    cd core && g++ -std=c++17 -O1 -fsanitize=address,undefined cheats.cpp layout.cpp selftest.cpp -o selftest && ./selftest

## Notas sobre trucos
- Mismos codigos Action Replay que en DraStic. Subconjunto soportado: tipos 0,1,2, 3-A (condicionales
  sobre la linea siguiente), B, C/D1 (bucle), D0 y D2-DC, E. El tipo F y otros se ignoran.
- Funcionan los trucos que modifican DATOS en RAM. Los que parchean CODIGO del juego no pueden
  funcionar en un recomp estatico, porque ese codigo ya esta convertido a C.
- Hoy el motor aplica los trucos sobre una memoria vacia: tendran efecto cuando exista la capa de hardware.

## Probar el lifter
    pip install capstone unicorn
    python3 tools/arm_lift.py salida/arm9.bin 0x02000000 stats
    python3 tools/arm_lift.py salida/arm9.bin 0x02000000 lift 0x02072b6c
    python3 tools/lift_selftest.py salida/arm9.bin   # compila y compara con un emulador ARM

## Compilar el APK
1. Instala Android Studio; en SDK Manager instala **NDK** y **CMake**.
2. File > Open > carpeta `android/` (Studio crea el Gradle wrapper al sincronizar).
   Si tu NDK es otro, cambia `ndkVersion` en `app/build.gradle.kts`.
3. Conecta el movil (depuracion USB) y pulsa Run, o Build > Build APK(s).
Hoy la app solo pide la ROM, la verifica, la extrae y abre la pantalla de juego vacia.
