#include "AnypointShader.h"
#include <pangolin/gl/glplatform.h>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace dso { namespace IOWrap {

static const char* kFragFiles[AnypointShader::NUM_MODES] = {
    "anypoint_mode1.gl.frag",
    "anypoint_mode2.gl.frag",
    "panorama_tube.gl.frag",
    "panorama_car.gl.frag",
};

static bool readFile(const std::string& path, std::string& out) {
    std::ifstream f(path);
    if (!f.good()) return false;
    std::stringstream ss; ss << f.rdbuf();
    out = ss.str();
    return !out.empty();
}

bool AnypointShader::compileOne(int idx, const std::string& fragPath) {
    const std::string vertPath = shaderDir_ + "/fullscreen.vert";
    std::string vertSrc, fragSrc;
    if (!readFile(vertPath, vertSrc)) {
        fprintf(stderr, "[anypoint] cannot read %s\n", vertPath.c_str());
        return false;
    }
    if (!readFile(fragPath, fragSrc)) {
        fprintf(stderr, "[anypoint] cannot read %s\n", fragPath.c_str());
        return false;
    }
    // GlSlProgram has no move-assignment, so we own each program via unique_ptr.
    auto fresh = std::make_unique<pangolin::GlSlProgram>();
    if (!fresh->AddShader(pangolin::GlSlVertexShader,   vertSrc)) return false;
    if (!fresh->AddShader(pangolin::GlSlFragmentShader, fragSrc)) return false;
    if (!fresh->Link())                                            return false;
    progs_[idx] = std::move(fresh);
    return true;
}

bool AnypointShader::init(const std::string& shaderDir) {
    shaderDir_ = shaderDir;
    if (vao_ == 0) glGenVertexArrays(1, &vao_);
    return reload();
}

bool AnypointShader::reload() {
    bool ok = true;
    for (int i = 0; i < NUM_MODES; ++i) {
        const std::string path = shaderDir_ + "/" + kFragFiles[i];
        bool one = compileOne(i, path);
        if (!one) {
            fprintf(stderr, "[anypoint] failed to build %s\n", kFragFiles[i]);
            ok = false;
        }
    }
    ready_ = ok;
    if (ok) fprintf(stderr, "[anypoint] all 4 shaders compiled OK\n");
    return ok;
}

void AnypointShader::render(Mode m, pangolin::GlTexture& tex,
                            const Calib& cal, const Controls& ctrl) {
    if (!ready_ || m < 0 || m >= NUM_MODES) return;

    glActiveTexture(GL_TEXTURE0);
    tex.Bind();

    if (!progs_[(int)m]) return;
    pangolin::GlSlProgram& p = *progs_[(int)m];
    p.Bind();

    p.SetUniform("tex", 0);
    p.SetUniform("camera_iCx",              cal.iCx);
    p.SetUniform("camera_iCy",              cal.iCy);
    p.SetUniform("camera_ratio",            cal.ratio);
    p.SetUniform("camera_imageWidth",       cal.imageWidth);
    p.SetUniform("camera_imageHeight",      cal.imageHeight);
    p.SetUniform("camera_calibrationRatio", cal.calibrationRatio);
    p.SetUniform("camera_para0",            cal.p[0]);
    p.SetUniform("camera_para1",            cal.p[1]);
    p.SetUniform("camera_para2",            cal.p[2]);
    p.SetUniform("camera_para3",            cal.p[3]);
    p.SetUniform("camera_para4",            cal.p[4]);
    p.SetUniform("camera_para5",            cal.p[5]);

    // Modes 1/2 also need sensor dimensions.
    if (m == ANYPOINT_MODE1 || m == ANYPOINT_MODE2) {
        p.SetUniform("camera_SensorWidth",  cal.sensorWidth);
        p.SetUniform("camera_SensorHeight", cal.sensorHeight);
    }

    // Per-mode control uniform names.
    switch (m) {
        case ANYPOINT_MODE1:
            p.SetUniform("alphaOffset", ctrl.param1);
            p.SetUniform("betaOffset",  ctrl.param2);
            p.SetUniform("zoom",        ctrl.param3);
            break;
        case ANYPOINT_MODE2:
            p.SetUniform("thetaX_degree", ctrl.param1);
            p.SetUniform("thetaY_degree", ctrl.param2);
            p.SetUniform("zoom",          ctrl.param3);
            break;
        case PANORAMA_TUBE:
        case PANORAMA_CAR:
            p.SetUniform("param1", ctrl.param1);
            p.SetUniform("param2", ctrl.param2);
            p.SetUniform("param3", ctrl.param3);
            break;
        default: break;
    }

    // Texture sampling settings best for live video.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);

    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);   // fullscreen triangle (vertex shader builds positions)
    glBindVertexArray(0);

    p.Unbind();
    tex.Unbind();
}

}} // namespace dso::IOWrap
