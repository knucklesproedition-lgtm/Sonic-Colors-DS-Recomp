# Sonic Colors DS (USA) -> Android — primera version jugable (en pruebas)

Necesitas **tu propia ROM** (`Sonic Colors (USA) (En,Ja,Fr,De,Es,It).nds`, codigo BXSE,
SHA-1 `fc76c4f3480b00f02646acb88d6713baf1b1d62a`). No incluyas la ROM ni sus datos en ningun repositorio ni APK.
**Para compilar el APK en GitHub: `LEEME_GITHUB.md`.**

## Que es esto (honesto)
Es un **emulador de Nintendo DS hecho a medida para este juego** (interprete ARM9/ARM7, GPU 2D/3D, hardware), con
extras (botones en pantalla, trucos, ajustes de pantalla, escala 3D). El recomp estatico (lifter ARM->C) queda como
prototipo: el juego corre por el interprete.

## Estado verificado (en PC, con la ROM real, volcando las pantallas a imagen)
- Arranque completo: descompresion del ARM9, SDK, sistema de archivos, lectura de overlays del cartucho.
- Pantallas de arranque: *Initializing save data* -> Nintendo -> ESRB -> SEGA -> SONIC TEAM -> mobiclip.
- Video de introduccion (mobiclip) a todo color y subtitulos; se salta con START.
- **Menu principal** (Play Game / Versus Mode / Time Attack / Options) y **tutorial del primer nivel** (Sonic, Tails, HUD).
- Guardado en EEPROM de 8 KB (persistente), firmware/flash, bus SPI, reloj tactil, FIFO entre CPU, DMA, temporizadores.
- ~100 fps equivalentes en menus en un PC; el interprete es ~2,8x mas rapido que la primera version.
- Pruebas automaticas: interprete ARM9 vs Unicorn/ARM946 sin diferencias (~100.000 instrucciones); 118 comprobaciones de
  trucos, GPU, hardware y video; todo compila con clang (el compilador de Android) y da la misma imagen que gcc.

## Novedades de esta version (v1.2)
- **Modo horizontal**: pantallas una al lado de la otra a pantalla completa, botones encima. Orientacion automatica/vertical/horizontal en Ajustes.
- **Editor de controles**: posicion y tamano de cada control, por separado en horizontal y en vertical (Ajustes > Editar posicion y tamano).
- **Piso que no se veia**: eran los fondos extendidos (BG2/BG3 en modo 5). Ahora se dibujan: afines, extendidos (mapa de 16 bits, bitmap, color directo),
  sprites rotados y mezcla alfa/fundidos. En el tutorial ya aparecen el piso y los totems.
- **3D**: iluminacion difusa/ambiental (hasta 4 luces) y sombreado toon (tabla de 32 colores). Pensado para que Sonic deje de verse como silueta negra.
- **Sonido**: los 16 canales de la ARM7 (PCM8, PCM16, ADPCM, PSG, ruido) mezclados a 32728 Hz estereo, reproducidos con AudioTrack.
  Verificado con la ROM real volcando el audio a WAV: musica sin saturar y con notas reconocibles. (No lo he podido ESCUCHAR.)
- **Rendimiento**: resolucion 3D por defecto 1x; salto de fotogramas automatico (omite video+3D cuando un fotograma tarda >20 ms; maximo 2 seguidos).
- **Menu nuevo** con pastillas (estilo Sonic R Android Edition), adaptable a horizontal/vertical.

## NO verificado (no tengo Android SDK ni dispositivo)
- Que el Kotlin nuevo compile (editor de controles, menu, audio, modo horizontal): se reviso a mano, pero nunca se ha compilado.
- Que la iluminacion/toon arreglen a Sonic en la escena concreta que viste (se probo con escenas sinteticas, no con esa escena).
- Que el salto de fotogramas baste para las fases especiales en tu movil.
- Que el APK **compile** (la interfaz Kotlin nunca se ha compilado) y que corra bien en un movil.
- La velocidad real en un movil (el interprete puede ser lento en gama baja: el juego iria a camara lenta).
- El presentador OpenGL ES (reescalado/filtros), los botones tactiles y el mapeo del toque en un dispositivo real.
- Que el parche de **pantalla unica** haga lo esperado durante un nivel normal: se aplica sin errores y el juego sigue
  corriendo, pero en el tutorial no cambia nada visible.

## NO implementado
- Display capture (la SEGUNDA parte de la cinematica de introduccion se ve con ruido; se puede saltar con START). Ventanas, mosaico, paletas extendidas.
  3D: brillo especular, highlight, niebla, antialiasing, contornos, matriz de textura. WiFi. Escenas pueden verse incompletas.
- Super Sonic: la logica esta probada pero esta **inerte** hasta descubrir las direcciones de RAM del juego.

## Los bloqueos que se resolvieron (por si vuelven a aparecer)
1. `OS_GetBootType` (0x027FFC40) debe valer 1 y la cabecera de la ROM estar en 0x027FFA80: sin eso `FS_Init` no monta la ROM y
   el overlay 19 nunca se cargaba.
2. SPI del ARM7 (energia, firmware, tactil); la flash del firmware exige el protocolo WREN/RDSR (bit WEL), si no el ARM9 reintenta siempre.
3. EEPROM de guardado por AUXSPI; sin ella el juego muestra "The save data could not be accessed".
4. Aviso de la cola de geometria 3D (IE bit 21) con retardo: la rutina de servicio borra el aviso al terminar.
5. Trucos AR: un condicional falso salta **hasta D0/D2**; y los trucos solo se aplican cuando el ARM9 ya se descomprimio
   (si no, se escribe sobre datos comprimidos y el juego se estrella).

## Herramientas
- `core/debug/shot.cpp`: ejecuta la ROM N unidades, simula pulsaciones y guarda las pantallas (PPM). Ejemplo:
      g++ -std=c++17 -O3 -Icore core/arm_cpu.cpp core/ds_system.cpp core/ds_video.cpp core/gpu3d.cpp core/nds_rom.cpp core/cheats.cpp core/debug/shot.cpp -o shot
      ./shot rom.nds 1500000000 /tmp/cap 100 "600:20:8,700:20:8"      # START (mascara 8) en t=600M y 700M unidades; captura cada 100M
  Mascaras: A=1 B=2 SELECT=4 START=8 DER=10 IZQ=20 ARR=40 ABA=80 R=100 L=200 X=400 Y=800 (hex). Variable `SHOT_CHEAT="XXXXXXXX YYYYYYYY ..."` aplica un truco.
- `core/debug/*`: trazas (llamadas, FIFO, DMA, IRQ, SPI...), usadas para diagnosticar los bloqueos de arriba.
- `tools/`: `nds_extract.py`, `arm_lift.py` (prototipo de lifter), `arm_cpu_difftest.py` (interprete vs Unicorn).

## Pruebas (PC)
    cd core
    SRC="arm_cpu.cpp ds_system.cpp ds_video.cpp gpu3d.cpp nds_rom.cpp"
    g++ -std=c++17 -O1 -fsanitize=address,undefined cheats.cpp layout.cpp selftest.cpp -o t1 && ./t1
    g++ -std=c++17 -O1 -fsanitize=address,undefined $SRC cheats.cpp mods.cpp selftest2.cpp -o t2 && ./t2
    g++ -std=c++17 -O1 -fsanitize=address,undefined gpu3d.cpp selftest3.cpp -o t3 && ./t3
    g++ -std=c++17 -O1 -fsanitize=address,undefined $SRC selftest4.cpp -o t4 && ./t4
    g++ -std=c++17 -O2 -shared -fPIC -w arm_cpu.cpp test_capi.cpp -o libarmtest.so && python3 ../tools/arm_cpu_difftest.py ./libarmtest.so 60000 40000 1

## Trucos
Action Replay: tipos 0,1,2, 3-A (si es FALSO se salta todo hasta el siguiente D0/D2), B, C/D1, D0, D2-DC, E. Tipo F ignorado.
Funcionan trucos de datos y de codigo (el interprete ejecuta la memoria parcheada). Solo se aplican tras el arranque del ARM9.
Viene preinstalado (desactivado) el parche **"Pantalla unica (Fix main 2D screen)"** del codigo que aportaste.

## Privacidad
La app guarda una copia privada de tu ROM y tu partida (`save.sav`) en su almacenamiento interno. Nunca las compartas ni las subas a un repositorio.
