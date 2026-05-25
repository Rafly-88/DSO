#pragma once
#include <pangolin/gl/glsl.h>
#include <pangolin/gl/gl.h>
#include <memory>
#include <string>

namespace dso { namespace IOWrap {

// Renders the live fisheye texture through one of four anypoint / panorama
// fragment shaders ported from the Flutter MobileApp. The shader files live in
// `shaders/` (relative to the binary's working directory) so they can be
// edited and reloaded at runtime via reload().
class AnypointShader {
public:
    enum Mode {
        ANYPOINT_MODE1 = 0,
        ANYPOINT_MODE2 = 1,
        PANORAMA_TUBE  = 2,
        PANORAMA_CAR   = 3,
        NUM_MODES
    };

    // Camera polynomial intrinsics (per-lens calibration). Sensible defaults
    // are filled in; replace with values from your fisheye calibration.
    struct Calib {
        float sensorWidth      = 4.8f;
        float sensorHeight     = 3.6f;
        float iCx              = 320.0f;   // image-centre X in pixels
        float iCy              = 240.0f;   // image-centre Y in pixels
        float ratio            = 1.0f;
        float imageWidth       = 640.0f;
        float imageHeight      = 480.0f;
        float calibrationRatio = 1.0f;
        // 6th-order polynomial (highest-order first). Values below are a
        // generic ~180° fisheye; replace with your own calibration.
        float p[6] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 200.0f };
    };

    // The three runtime knobs exposed to the Pangolin viewer. Meaning depends
    // on the active mode (see header comments below).
    struct Controls {
        float param1 = 0.0f;   // mode1:alphaOffset  mode2:thetaX  tube:alpha_min   car:alpha_max
        float param2 = 0.0f;   // mode1:betaOffset   mode2:thetaY  tube:alpha_max   car:iC_alpha
        float param3 = 1.0f;   // mode1/2:zoom                     tube:unused      car:iC_beta
    };

    AnypointShader() = default;
    ~AnypointShader() = default;

    // Must be called from the Pangolin thread (GL context current).
    // shaderDir defaults to "shaders". Returns false if any file is missing
    // or fails to compile.
    bool init(const std::string& shaderDir = "shaders");

    // Re-compile programs from disk (live editing). Safe to call any time
    // from the Pangolin thread.
    bool reload();

    // Renders the chosen mode covering the currently active Pangolin view.
    // `tex` must already exist; we'll bind it to texture unit 0 as `tex`.
    void render(Mode m, pangolin::GlTexture& tex,
                const Calib& cal, const Controls& ctrl);

    bool ready() const { return ready_; }

private:
    bool compileOne(int idx, const std::string& fragPath);

    std::string                            shaderDir_;
    std::unique_ptr<pangolin::GlSlProgram> progs_[NUM_MODES];
    unsigned int                           vao_   = 0;
    bool                                   ready_ = false;
};

}} // namespace dso::IOWrap
