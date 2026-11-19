#include "gl_present.h"
#include <GLES3/gl3.h>
#include <android/log.h>

namespace nds {

static const char* kVert = R"(#version 300 es
layout(location = 0) in vec2 pos;
uniform vec4 rect;   // x, y, ancho, alto en pixeles de la vista (y desde arriba)
uniform vec2 view;
out vec2 uv;
void main() {
    vec2 p = rect.xy + pos * rect.zw;
    gl_Position = vec4(p.x / view.x * 2.0 - 1.0, 1.0 - p.y / view.y * 2.0, 0.0, 1.0);
    uv = pos;
}
)";

static const char* kFrag = R"(#version 300 es
precision highp float;
uniform sampler2D tex;
uniform vec2 texSize;
uniform float outScale;   // pixeles de salida por texel
uniform int mode;
in vec2 uv;
out vec4 color;
vec3 catmullRom(vec2 uvIn) {          // bicubico de 9 lecturas (Catmull-Rom): bordes suaves sin desenfocar de mas
    vec2 samplePos = uvIn * texSize;
    vec2 texPos1 = floor(samplePos - 0.5) + 0.5;
    vec2 f = samplePos - texPos1;
    vec2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f));
    vec2 w1 = 1.0 + f * f * (-2.5 + 1.5 * f);
    vec2 w2 = f * (0.5 + f * (2.0 - 1.5 * f));
    vec2 w3 = f * f * (-0.5 + 0.5 * f);
    vec2 w12 = w1 + w2;
    vec2 offset12 = w2 / w12;
    vec2 p0 = (texPos1 - 1.0) / texSize;
    vec2 p3 = (texPos1 + 2.0) / texSize;
    vec2 p12 = (texPos1 + offset12) / texSize;
    vec3 r = vec3(0.0);
    r += texture(tex, vec2(p0.x, p0.y)).rgb * w0.x * w0.y;
    r += texture(tex, vec2(p12.x, p0.y)).rgb * w12.x * w0.y;
    r += texture(tex, vec2(p3.x, p0.y)).rgb * w3.x * w0.y;
    r += texture(tex, vec2(p0.x, p12.y)).rgb * w0.x * w12.y;
    r += texture(tex, vec2(p12.x, p12.y)).rgb * w12.x * w12.y;
    r += texture(tex, vec2(p3.x, p12.y)).rgb * w3.x * w12.y;
    r += texture(tex, vec2(p0.x, p3.y)).rgb * w0.x * w3.y;
    r += texture(tex, vec2(p12.x, p3.y)).rgb * w12.x * w3.y;
    r += texture(tex, vec2(p3.x, p3.y)).rgb * w3.x * w3.y;
    return clamp(r, 0.0, 1.0);
}

void main() {
    if (mode == 3) { color = vec4(catmullRom(uv), 1.0); return; }
    vec2 uv2 = uv;
    if (mode == 2) {      // bilinear nitido: el interior de cada texel queda plano y solo se suaviza el borde
        vec2 p = uv * texSize;
        vec2 f = fract(p);
        vec2 w = clamp(0.5 + (f - 0.5) * outScale, 0.0, 1.0);
        uv2 = (floor(p) + w) / texSize;
    }
    color = vec4(texture(tex, uv2).rgb, 1.0);
}
)";

static GLuint compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[512]; glGetShaderInfoLog(s, sizeof log, nullptr, log); __android_log_print(ANDROID_LOG_ERROR, "SonicRecomp", "shader: %s", log); }
    return s;
}

bool GlPresenter::init() {
    GLuint vs = compile(GL_VERTEX_SHADER, kVert), fs = compile(GL_FRAGMENT_SHADER, kFrag);
    prog_ = glCreateProgram(); glAttachShader(prog_, vs); glAttachShader(prog_, fs); glLinkProgram(prog_);
    GLint ok = 0; glGetProgramiv(prog_, GL_LINK_STATUS, &ok);
    glDeleteShader(vs); glDeleteShader(fs);
    if (!ok) { ready_ = false; return false; }
    loc_rect_ = glGetUniformLocation(prog_, "rect"); loc_view_ = glGetUniformLocation(prog_, "view");
    loc_size_ = glGetUniformLocation(prog_, "texSize"); loc_scale_ = glGetUniformLocation(prog_, "outScale");
    loc_mode_ = glGetUniformLocation(prog_, "mode"); loc_tex_ = glGetUniformLocation(prog_, "tex");
    static const float quad[8] = {0, 0, 1, 0, 0, 1, 1, 1};
    glGenVertexArrays(1, &vao_); glBindVertexArray(vao_);
    glGenBuffers(1, &vbo_); glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glGenTextures(2, tex_);
    for (int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, tex_[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    ready_ = true;
    return true;
}

void GlPresenter::shutdown() {
    if (!ready_) return;
    glDeleteTextures(2, tex_); glDeleteBuffers(1, &vbo_); glDeleteVertexArrays(1, &vao_); glDeleteProgram(prog_);
    ready_ = false;
}

void GlPresenter::draw_one(int idx, const uint32_t* px, int w, int h, const Rect& r, int vw, int vh, int filter) {
    if (!r.visible || !px) return;
    glBindTexture(GL_TEXTURE_2D, tex_[idx]);
    GLint f = (filter == 0) ? GL_NEAREST : GL_LINEAR;                    // modo 2 usa LINEAR y corrige las coordenadas en el shader
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, f);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, f);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glUniform4f(loc_rect_, r.x, r.y, r.w, r.h);
    glUniform2f(loc_size_, (float)w, (float)h);
    glUniform1f(loc_scale_, r.w / (float)w);
    glUniform1i(loc_mode_, filter);
    (void)vw; (void)vh;
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void GlPresenter::draw(const uint32_t* top, const uint32_t* bottom, int w, int h, const Layout& l, int vw, int vh, int filter) {
    if (!ready_) return;
    glDisable(GL_SCISSOR_TEST); glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND);
    glUseProgram(prog_); glBindVertexArray(vao_);
    glActiveTexture(GL_TEXTURE0); glUniform1i(loc_tex_, 0);
    glUniform2f(loc_view_, (float)vw, (float)vh);
    draw_one(0, top, w, h, l.top, vw, vh, filter);
    draw_one(1, bottom, w, h, l.bottom, vw, vh, filter);
}

}  // namespace nds
