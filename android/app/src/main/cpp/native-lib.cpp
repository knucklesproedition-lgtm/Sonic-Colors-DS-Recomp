#include <jni.h>
#include <android/log.h>
#include <atomic>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>
#include "cheats.h"
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
        });
    return env->NewStringUTF(err.c_str());
}

// ---------------- estado compartido del motor ----------------
static std::atomic<bool> g_started{false};
static std::atomic<int> g_mode{nds::LAYOUT_STACKED};
static std::atomic<bool> g_swapped{false};
static std::atomic<float> g_ratio{0.65f};
static std::atomic<uint32_t> g_keys{0};          // mascara de nds::Key (1 = pulsado)
static std::atomic<int> g_touch_x{0}, g_touch_y{0};
static std::atomic<bool> g_touch_down{false};
static std::mutex g_cheat_mutex;
static nds::CheatEngine g_cheats;

// Memoria provisional: cuando exista la capa de hardware, aqui va la RAM/IO reales de DS.
struct NullMem : nds::MemIf {
    uint32_t r32(uint32_t) override { return 0; }
    uint32_t r16(uint32_t) override { return 0; }
    uint32_t r8(uint32_t) override { return 0; }
    void w32(uint32_t, uint32_t) override {}
    void w16(uint32_t, uint32_t) override {}
    void w8(uint32_t, uint32_t) override {}
};

extern "C" JNIEXPORT void JNICALL
Java_com_sonicrecomp_app_NativeBridge_engineInit(JNIEnv* env, jclass, jstring dataDir) {
    LOG("engineInit: %s", jstr(env, dataDir).c_str());
    // TODO: cargar arm9.bin/overlays en memoria emulada, inicializar GPU/audio, saltar a arm9_entry
    g_started = true;
}

extern "C" JNIEXPORT void JNICALL
Java_com_sonicrecomp_app_NativeBridge_engineFrame(JNIEnv*, jclass) {
    if (!g_started) return;
    // TODO: ejecutar un frame del juego recompilado; leer g_keys / g_touch_* como KEYINPUT y pantalla tactil
    {   // Los trucos AR se aplican una vez por fotograma, como en la consola
        static NullMem mem;   // TODO: sustituir por la memoria real
        std::lock_guard<std::mutex> lk(g_cheat_mutex);
        g_cheats.apply_frame(mem);
    }
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
