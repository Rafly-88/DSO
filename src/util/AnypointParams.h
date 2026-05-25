#pragma once
#include <mutex>
#include <string>

namespace dso {

// Runtime knobs for the fisheye anypoint projection. Written by the Pangolin
// viewer thread (when sliders change) and read by the camera capture thread
// every frame, so all access goes through getAnypointParams/setAnypointParams.
//
// Field meaning depends on `mode`:
//   mode 0 (anypoint_mode1):  param1=alphaOffset  param2=betaOffset  param3=zoom
//   mode 1 (anypoint_mode2):  param1=thetaX_deg   param2=thetaY_deg  param3=zoom
//   mode 2 (panorama_tube):   param1=alpha_min    param2=alpha_max   param3=unused
//   mode 3 (panorama_car):    param1=alpha_max    param2=iC_alpha    param3=iC_beta
struct AnypointParams {
    int   mode             = 0;       // 0..3 (see above)
    float param1           = 0.0f;
    float param2           = 0.0f;
    float param3           = 1.0f;
    float iCx              = 320.0f;  // fisheye image-centre X
    float iCy              = 240.0f;  // fisheye image-centre Y
    float calibrationRatio = 1.0f;
    // Full 6-coefficient polynomial (highest-order first). poly5 mirrors p[5]
    // for backward compatibility with the existing Pangolin slider; loaders
    // should write the full vector when calibration data is available.
    float p[6]             = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 200.0f };
    float poly5            = 200.0f;
    bool  enabled          = true;    // false => pass fisheye straight through
};

// Read a 6-coefficient fisheye polynomial from a text file with format:
//   p0 p1 p2 p3 p4 p5
// (highest order first, single line, whitespace-separated). Returns true on
// success and overwrites AnypointParams.p[0..5] in the global state.
bool loadAnypointPolynomial(const std::string& path);

AnypointParams getAnypointParams();
void           setAnypointParams(const AnypointParams& p);

} // namespace dso
