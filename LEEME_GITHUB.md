# Compilar el APK en GitHub (sin instalar nada)

Necesitas una cuenta gratuita de GitHub. **No subas tu ROM.**

## Opcion A: desde el MOVIL (solo subes UN archivo + pegas un texto)
1. En github.com (navegador) crea un repositorio **privado** nuevo (por ejemplo `sonic-recomp`).
2. En el repositorio: **Add file > Upload files** y sube `SonicColorsRecomp_GitHub.zip` **sin descomprimirlo**. Pulsa *Commit changes*.
3. **Add file > Create new file**. En el nombre escribe exactamente:  `.github/workflows/build-apk.yml`
   (al escribir las barras `/` GitHub crea las carpetas). Pega dentro todo el contenido del archivo `build-apk.yml`. *Commit changes*.
4. Pestana **Actions** > **Build APK** > **Run workflow** (si ya empezo solo al hacer el commit, espera a que termine).
5. Tarda unos 5-10 minutos. Al terminar, entra en esa ejecucion y baja a **Artifacts**: descarga `SonicColorsRecomp-APK`.
6. Descomprime el .zip descargado, abre `app-debug.apk` e instalalo (Android pedira permitir "instalar apps desconocidas").

## Opcion B: desde un PC
Descomprime el zip y arrastra TODO su contenido (incluida la carpeta oculta `.github`) a *Add file > Upload files*.

## Si falla la compilacion
Abre la ejecucion en *Actions*, entra en el paso en rojo y copia el mensaje de error: mandamelo y lo corrijo.
(El Kotlin de la app nunca se ha compilado antes: puede haber algun error en el primer intento.)

## Primer uso
1. **Elegir ROM (.nds)** y selecciona tu ROM de Sonic Colors (USA). Verifica la version y prepara el juego (unos segundos).
2. En **Ajustes** elige disposicion de pantallas, resolucion 3D (1x recomendada al principio) y filtro.
3. **JUGAR**. Secuencia esperada: *Initializing save data* -> logos (Nintendo, ESRB, SEGA, SONIC TEAM, mobiclip) ->
   video de introduccion -> **pulsa START (boton en pantalla) para saltarlo** -> menu principal (toca "Play Game" o pulsa A).
   La primera vez veras "Initializing save data": es normal (crea tu partida en `save.sav`).
4. **Trucos > "Pantalla unica"**: activalo ANTES de pulsar JUGAR (se aplica al iniciar). Es experimental.

## Que esperar (version de prueba)
- **Sin sonido.** La velocidad depende de tu movil: si va a camara lenta, usa 1x y filtro "Pixeles".
- Algunas escenas pueden verse incompletas (ver README, "NO implementado").
- Cuentame que ves: capturas de pantalla, en que momento se atasca, y si va rapido o lento.
