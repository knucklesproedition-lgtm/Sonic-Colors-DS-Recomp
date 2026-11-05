#include <jni.h>
#include <android/log.h>
#include <atomic>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>
#include "cheats.h"
#include "ds_system.h"
#include "gl_present.h"
#include "mods.h"
#include <cstdio>
#include <fstream>
#include <iterator>
#include <memory>
#include "keys.h"
#include "layout.h"
#include "nds_rom.h"

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "SonicRecomp", __VA_ARGS__)

static std::string jstr(JNIEnv* env, jstring s) {
    const char* c = env->GetStringUTFChars(s, nullptr);
    std::string r(c);
    env->ReleaseStringUTFChars(s, c);
    return r;
}

// ---------------- extraccion de ROM ----------------
extern "C" JNIEXPORT jstring JNICALL
Java_com_sonicrecomp_app_NativeBridge_extractRom(JNIEnv* env, jclass, jstring romPath,
                                                 jstring outDir, jobject cb) {
    jclass cls = env->GetObjectClass(cb);
    jmethodID mid = env->GetMethodID(cls, "onProgress", "(ILjava/lang/String;)V");
    std::string err = nds::extract_rom(jstr(env, romPath), jstr(env, outDir),
        [&](int pc, const std::string& msg) {
            jstring m = env->NewStringUTF(msg.c_str());
            env->CallVoidMethod(cb, mid, pc, m);
            env->DeleteLocalRef(m);
        }, /*extract_files=*/false);       // ARM9, ARM7 y overlays si; los ~500 archivos (~60 MB) no hacen falta: el motor lee la ROM
    return env->NewStringUTF(err.c_str());
}

// ---------------- estado compartido del motor ----------------
static std::atomic<int> g_mode{nds::LAYOUT_STACKED};
static std::atomic<bool> g_swapped{false};
static std::atomic<float> g_ratio{0.65f};
static std::atomic<uint32_t> g_keys{0};          // mascara de nds::Key (1 = pulsado)
static std::atomic<int> g_touch_x{0}, g_touch_y{0};
static std::atomic<bool> g_touch_down{false};
static std::mutex g_cheat_mutex;
static nds::CheatEngine g_cheats;

// La consola ocupa ~6 MB: SIEMPRE en el monton, nunca en la pila.
static std::unique_ptr<nds::Console> g_console;
static nds::ModHost g_mods;
static std::atomic<bool> g_running{false};
static uint64_t g_frames_run = 0;
static nds::GlPresenter g_gl;
static std::string g_data_dir;
static std::atomic<int> g_view_w{1}, g_view_h{1}, g_scale{2}, g_filter{2};

// Memoria del ARM9 vista por trucos y mods: es la misma que ejecuta el interprete.
struct Arm9Mem : nds::MemIf {
    nds::Console& c; explicit Arm9Mem(nds::Console& k) : c(k) {}
    uint32_t r32(uint32_t a) override { return c.bus9.read32(a & ~3u); }
    uint32_t r16(uint32_t a) override { return c.bus9.read16(a & ~1u); }
    uint32_t r8(uint32_t a) override { return c.bus9.read8(a); }
    void w32(uint32_t a, uint32_t v) override { c.bus9.write32(a & ~3u, v); }
    void w16(uint32_t a, uint32_t v) override { c.bus9.write16(a & ~1u, v); }
    void w8(uint32_t a, uint32_t v) override { c.bus9.write8(a, v); }
};

static void save_backup() {
    if (!g_console || !g_console->backup_dirty || g_data_dir.empty()) return;
    std::ofstream o(g_data_dir + "/save.sav.tmp", std::ios::binary | std::ios::trunc);
    const auto& b = g_console->backup();
    o.write((const char*)b.data(), (std::streamsize)b.size()); o.close();
    if (o) { std::rename((g_data_dir + "/save.sav.tmp").c_str(), (g_data_dir + "/save.sav").c_str()); g_console->backup_dirty = false; }
}

extern "C" JNIEXPORT void JNICALL
Java_com_sonicrecomp_app_NativeBridge_engineFlush(JNIEnv*, jclass) { save_backup(); }          // al pausar/cerrar la app

extern "C" JNIEXPORT void JNICALL
Java_com_sonicrecomp_app_NativeBridge_engineInit(JNIEnv* env, jclass, jstring dataDir) {
    std::string dir = jstr(env, dataDir);
    g_data_dir = dir;
    g_running = false; g_frames_run = 0;
    std::ifstream f(dir + "/rom.nds", std::ios::binary);
    std::vector<uint8_t> rom((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (rom.empty()) { LOG("engineInit: no hay rom.nds en %s", dir.c_str()); return; }
    g_console.reset(new nds::Console());
    g_console->set_graphics(g_scale, g_filter);
    if (!g_console->load_rom(rom)) { LOG("engineInit: ROM no valida"); g_console.reset(); return; }
    {   // partida guardada (EEPROM de 8 KB del cartucho)
        std::ifstream sf(dir + "/save.sav", std::ios::binary);
        std::vector<uint8_t> sd((std::istreambuf_iterator<char>(sf)), std::istreambuf_iterator<char>());
        if (!sd.empty()) g_console->set_backup(sd);
    }
    if (!g_mods.find("super_sonic")) g_mods.add(std::make_unique<nds::SuperSonicMod>(nds::SuperSonicConfig()));
    g_running = true;
    LOG("engineInit: ROM cargada, ARM9 entry=%08x", g_console->cpu9.pc());
}

extern "C" JNIEXPORT void JNICALL
Java_com_sonicrecomp_app_NativeBridge_engineFrame(JNIEnv*, jclass) {
    if (!g_running || !g_console) return;
    Arm9Mem mem(*g_console);
    g_console->keys = g_keys;
    g_console->set_touch(g_touch_x, g_touch_y, g_touch_down);
    if (g_console->arm9_booted()) {   // trucos y mods solo cuando el ARM9 ya descomprimio su codigo (antes la memoria aun es datos comprimidos)
        std::lock_guard<std::mutex> lk(g_cheat_mutex);
        g_cheats.apply_frame(mem);
        g_mods.run_frame(mem, g_keys);
    }
    g_console->run_units(560190);                         // un fotograma de DS (263 lineas x 2130 ciclos)
    nds::Layout lay = nds::compute_layout((float)g_view_w, (float)g_view_h, g_mode, g_swapped, g_ratio);
    g_gl.draw(g_console->screen_pixels(0), g_console->screen_pixels(1), g_console->screen_w(), g_console->screen_h(), lay, g_view_w, g_view_h, g_console->gfx_filter);
    if (g_frames_run % 300 == 299) save_backup();                  // guarda la partida si cambio (cada ~5 s)
    if (++g_frames_run % 120 == 0)
        LOG("frame %llu: ARM9 pc=%08x ARM7 pc=%08x", (unsigned long long)g_frames_run, g_console->cpu9.pc(), g_console->cpu7.pc());
}

extern "C" JNIEXPORT void JNICALL
Java_com_sonicrecomp_app_NativeBridge_engineGlInit(JNIEnv*, jclass) { g_gl.init(); }       // el contexto de GL se (re)crea: reconstruir recursos

extern "C" JNIEXPORT void JNICALL
Java_com_sonicrecomp_app_NativeBridge_engineSetViewport(JNIEnv*, jclass, jint w, jint h) { g_view_w = w; g_view_h = h; }

extern "C" JNIEXPORT void JNICALL
Java_com_sonicrecomp_app_NativeBridge_engineSetGraphics(JNIEnv*, jclass, jint scale, jint filter) {
    g_scale = scale; g_filter = filter;
    if (g_console) g_console->set_graphics(scale, filter);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_sonicrecomp_app_NativeBridge_engineIsRunning(JNIEnv*, jclass) { return g_running ? JNI_TRUE : JNI_FALSE; }

extern "C" JNIEXPORT void JNICALL
Java_com_sonicrecomp_app_NativeBridge_modsSetSuperSonic(JNIEnv*, jclass, jboolean on) {
    if (nds::Mod* m = g_mods.find("super_sonic")) m->enabled = on;
}

// ---------------- botones y pantalla tactil ----------------
extern "C" JNIEXPORT void JNICALL
Java_com_sonicrecomp_app_NativeBridge_engineSetKeys(JNIEnv*, jclass, jint mask) {
    g_keys = (uint32_t)mask;
}

// x,y en pixeles de la vista de pantallas; se convierte con la disposicion actual.
extern "C" JNIEXPORT void JNICALL
Java_com_sonicrecomp_app_NativeBridge_engineTouchView(JNIEnv*, jclass, jfloat viewW, jfloat viewH,
                                                      jfloat x, jfloat y, jboolean down) {
    if (!down) { g_touch_down = false; return; }
    nds::Layout l = nds::compute_layout(viewW, viewH, g_mode, g_swapped, g_ratio);
    int dx, dy;
    if (nds::map_touch(l, x, y, &dx, &dy)) { g_touch_x = dx; g_touch_y = dy; g_touch_down = true; }
    else g_touch_down = false;
}

// ---------------- disposicion de pantallas ----------------
extern "C" JNIEXPORT void JNICALL
Java_com_sonicrecomp_app_NativeBridge_engineSetLayout(JNIEnv*, jclass, jint mode, jboolean swapped, jfloat ratio) {
    g_mode = mode; g_swapped = swapped; g_ratio = ratio;
}

// [top.x, top.y, top.w, top.h, top.visible, bottom.x, bottom.y, bottom.w, bottom.h, bottom.visible]
extern "C" JNIEXPORT jfloatArray JNICALL
Java_com_sonicrecomp_app_NativeBridge_engineGetLayout(JNIEnv* env, jclass, jfloat viewW, jfloat viewH) {
    nds::Layout l = nds::compute_layout(viewW, viewH, g_mode, g_swapped, g_ratio);
    float v[10] = {l.top.x, l.top.y, l.top.w, l.top.h, l.top.visible ? 1.f : 0.f,
                   l.bottom.x, l.bottom.y, l.bottom.w, l.bottom.h, l.bottom.visible ? 1.f : 0.f};
    jfloatArray arr = env->NewFloatArray(10);
    env->SetFloatArrayRegion(arr, 0, 10, v);
    return arr;
}

// ---------------- trucos ----------------
extern "C" JNIEXPORT void JNICALL
Java_com_sonicrecomp_app_NativeBridge_cheatsClear(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lk(g_cheat_mutex);
    g_cheats.clear();
}

// Devuelve true si el texto del codigo es valido y se anadio.
extern "C" JNIEXPORT jboolean JNICALL
Java_com_sonicrecomp_app_NativeBridge_cheatsAdd(JNIEnv* env, jclass, jstring name, jstring code, jboolean enabled) {
    std::vector<uint32_t> words;
    if (!nds::parse_code_text(jstr(env, code), words)) return JNI_FALSE;
    std::lock_guard<std::mutex> lk(g_cheat_mutex);
    g_cheats.add(jstr(env, name), words, enabled);
    return JNI_TRUE;
}

// Comprueba un codigo sin anadirlo (para validar lo que escribe el usuario).
extern "C" JNIEXPORT jboolean JNICALL
Java_com_sonicrecomp_app_NativeBridge_cheatsValidate(JNIEnv* env, jclass, jstring code) {
    std::vector<uint32_t> words;
    return nds::parse_code_text(jstr(env, code), words) ? JNI_TRUE : JNI_FALSE;
}

// Lee un archivo de trucos: devuelve [nombre0, codigo0, "1"/"0", nombre1, ...]
extern "C" JNIEXPORT jobjectArray JNICALL
Java_com_sonicrecomp_app_NativeBridge_cheatsParseFile(JNIEnv* env, jclass, jstring text) {
    auto list = nds::parse_cheat_file(jstr(env, text));
    jclass sc = env->FindClass("java/lang/String");
    jobjectArray arr = env->NewObjectArray((jsize)list.size() * 3, sc, nullptr);
    char buf[24];
    for (size_t i = 0; i < list.size(); i++) {
        std::string code;
        for (size_t k = 0; k + 1 < list[i].words.size(); k += 2) {
            std::snprintf(buf, sizeof buf, "%08X %08X\n", list[i].words[k], list[i].words[k + 1]);
            code += buf;
        }
        env->SetObjectArrayElement(arr, (jsize)(i * 3), env->NewStringUTF(list[i].name.c_str()));
        env->SetObjectArrayElement(arr, (jsize)(i * 3 + 1), env->NewStringUTF(code.c_str()));
        env->SetObjectArrayElement(arr, (jsize)(i * 3 + 2), env->NewStringUTF(list[i].enabled ? "1" : "0"));
    }
    return arr;
}
